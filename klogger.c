#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/mutex.h>

#define DEVICE_NAME "klogger"
#define CLASS_NAME "klogger"
#define LOG_BUF_LEN (1 << 5)
#define MSG_LEN 8 
#define MAX_ENTRIES (LOG_BUF_LEN / MSG_LEN);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lionel Silva");
MODULE_DESCRIPTION("Kernel-space Logger");
MODULE_VERSION("0.1");

struct klogger {
    char *log_buffer;
    size_t head;
    size_t tail;
    size_t log_lines;
    struct mutex log_mutex;
    atomic_t open_count;
    atomic_t entries;
    struct class *device_class;
    struct device *device;
    int major_number;
} klog_t;

static struct klogger klog;

static int dev_open(struct inode *inodep, struct file *filep);
static int dev_release(struct inode *inodep, struct file *filep);
static ssize_t dev_read(struct file *filep, char *user_buffer, size_t count, loff_t *file_pos);
static ssize_t dev_write(struct file *filep, const char *user_buffer, size_t count, loff_t *file_pos);

static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

static int dev_open(struct inode *inodep, struct file *filep) {
    // TODO
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    // TODO
    return 0;
}

static ssize_t dev_read(struct file *filep, char __user *user_buffer, size_t count, loff_t *file_pos) {
    //TODO
   return count;
}







static ssize_t dev_write(struct file *filep, const char __user *user_buffer, size_t count, loff_t *file_pos) {
    size_t msg_size = MSG_LEN;
    size_t max_entries = MAX_ENTRIES;

    // If incoming data is larger than the buffer, truncate to keep only the latest part
    if (count >= msg_size) {
        user_buffer += count - (msg_size - 1);
        count = msg_size - 1;
    }

    mutex_lock(&klog.log_mutex);

    memset(klog.log_buffer + (klog.head * msg_size), 0, msg_size);
    if (copy_from_user(&klog.log_buffer[klog.head * msg_size], user_buffer, count)) {
        mutex_unlock(&klog.log_mutex);
        return -EFAULT;
    }

    klog.log_buffer[(klog.head * msg_size) + count] = '\n';

    size_t log_entries = atomic_inc_return(&klog.entries);

    if (log_entries > max_entries) {
        log_entries = atomic_dec_return(&klog.entries);
    }

    klog.head = (klog.head + 1) & (max_entries - 1);
    if (log_entries == max_entries && klog.head == klog.tail) {
        klog.tail = (klog.tail + 1) & (max_entries - 1);
    }

    mutex_unlock(&klog.log_mutex);  // Unlock after writing


    return count; // Return number of bytes written
}




static int __init klogger_init(void) {

    // Initialize the device structure
    klog.log_buffer = kmalloc(LOG_BUF_LEN, GFP_KERNEL);
    memset(klog.log_buffer, 0, LOG_BUF_LEN);
    klog.head = 0;
    klog.tail = 0;
    klog.log_lines = 0;
    // Initialize synchronization primitives
    mutex_init(&klog.log_mutex);
    // rwlock_init(&klog.rwlock);
    atomic_set(&klog.open_count, 0);
    atomic_set(&klog.entries, 0);



    // Register major number
    klog.major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (klog.major_number < 0) {
        printk(KERN_ERR "Failed to register major number\n");
        return klog.major_number;
    }

    // Create device class
    klog.device_class = class_create(CLASS_NAME);
    if (IS_ERR(klog.device_class)) {
        unregister_chrdev(klog.major_number, DEVICE_NAME);
        printk(KERN_ERR "Failed to create device class\n");
        return PTR_ERR(klog.device_class);
    }

    // Create device
    klog.device = device_create(
        klog.device_class, 
        NULL, 
        MKDEV(klog.major_number, 0), 
        NULL, 
        DEVICE_NAME
    );
    if (IS_ERR(klog.device)) {
        class_destroy(klog.device_class);
        unregister_chrdev(klog.major_number, DEVICE_NAME);
        printk(KERN_ERR "Failed to create device\n");
        return PTR_ERR(klog.device);
    }

    printk(KERN_INFO "Klogger device registered\n");
    
    return 0;
}

static void __exit klogger_exit(void) {

    // Free buffer
    kfree(klog.log_buffer);

    // Destroy device
    device_destroy(klog.device_class, MKDEV(klog.major_number, 0));
    
    // Destroy class
    class_destroy(klog.device_class);
    
    // Unregister major number
    unregister_chrdev(klog.major_number, DEVICE_NAME);

    printk(KERN_INFO "Klogger unregistered\n");
}

module_init(klogger_init);
module_exit(klogger_exit);

