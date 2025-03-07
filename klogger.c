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
    size_t prev_head;
    struct mutex log_mutex;
    atomic_t open_count;
    atomic_t entries;
    struct class *device_class;
    struct device *device;
    int major_number;
    bool full;
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
    size_t msg_size = MSG_LEN;
    size_t max_entries = MAX_ENTRIES;

    if (!user_buffer || count == 0) {
        return -EINVAL;
    }

    if (*file_pos >= LOG_BUF_LEN) {
        return 0;
    }

    if (count > LOG_BUF_LEN) {
        count = LOG_BUF_LEN;
    }

    mutex_lock(&klog.log_mutex);
    size_t usr_idx = 0;
    size_t bytes_to_copy = 0;
    size_t i = klog.tail;

    while (usr_idx + bytes_to_copy <= count) {
        bytes_to_copy = strlen(klog.log_buffer + (i * msg_size));
        if (copy_to_user(user_buffer + usr_idx, klog.log_buffer + (i * msg_size), bytes_to_copy) != 0) {
            return -EFAULT;
        }
        usr_idx += bytes_to_copy;
        if (i == klog.prev_head) {
            break;
        }
        i = (i + 1) & (max_entries - 1); 
    }

    *file_pos = count;

    mutex_unlock(&klog.log_mutex);

    

    return count;
}







static ssize_t dev_write(struct file *filep, const char __user *user_buffer, size_t count, loff_t *file_pos) {
    size_t msg_size = MSG_LEN;
    size_t max_entries = MAX_ENTRIES;
    size_t bytes_to_copy = count;
    size_t usr_idx = 0;

    // If incoming data is larger than the buffer, truncate to keep only the latest part
    if (count >= msg_size) {
        usr_idx = count - (msg_size - 1);
        bytes_to_copy = msg_size - 1;
    }

    if (*file_pos >= msg_size) {
        return 0;
    }

    mutex_lock(&klog.log_mutex);

    if (atomic_read(&klog.entries) == max_entries && klog.head == klog.tail) {
        klog.tail = (klog.tail + 1) & (max_entries - 1);
    }
    memset(klog.log_buffer + (klog.head * msg_size), '\0', msg_size);
    if (copy_from_user(klog.log_buffer + (klog.head * msg_size), user_buffer + usr_idx, bytes_to_copy)) {
        mutex_unlock(&klog.log_mutex);
        return -EFAULT;
    }

    klog.log_buffer[(klog.head * msg_size) + bytes_to_copy] = '\0';


    if (atomic_read(&klog.entries) < max_entries) {
        atomic_inc(&klog.entries);
    }

    klog.prev_head = klog.head;

    klog.head = (klog.head + 1) & (max_entries - 1);
    

    *file_pos = count;

    mutex_unlock(&klog.log_mutex);  // Unlock after writing



    return count; // Return number of bytes written
}




static int __init klogger_init(void) {

    // Initialize the device structure
    klog.log_buffer = kmalloc(LOG_BUF_LEN, GFP_KERNEL);
    memset(klog.log_buffer, 0, LOG_BUF_LEN);
    klog.head = 0;
    klog.tail = 0;
    klog.prev_head = 0;
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

