/*
* klogger.c - A simple kernel-space circular buffer logger
*
* This module implements a character device driver that provides a circular buffer
* for logging messages in kernel space. It supports concurrent access through
* read-write locks and maintains a fixed-size buffer of messages.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/mutex.h>

/* Device configuration */
#define DEVICE_NAME "klogger"    /* Name of the device in /dev */
#define CLASS_NAME "klogger"     /* Name of the device class */
#define LOG_BUF_LEN (1 << 18)    /* Total buffer size (32 bytes) */
#define MSG_LEN 256               /* Maximum length of each message */
#define MAX_ENTRIES (LOG_BUF_LEN / MSG_LEN)  /* Maximum number of messages in buffer */

/* Module metadata */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lionel Silva");
MODULE_DESCRIPTION("Kernel-space Logger");
MODULE_VERSION("0.1");

/**
 * struct klogger - Main data structure for the kernel logger
 * @log_buffer: Circular buffer to store messages
 * @head: Index where next write will occur
 * @tail: Index where next read will start
 * @prev_head: Previous head position for read operations
 * @rwlock: Read-write lock for thread safety
 * @open_count: Number of processes currently using the device
 * @entries: Current number of valid entries in the buffer
 * @device_class: Pointer to the device class
 * @device: Pointer to the device structure
 * @major_number: Major number assigned to the device
 */
struct klogger {
    char log_buffer[LOG_BUF_LEN];
    size_t head;
    size_t tail;
    size_t prev_head;
    rwlock_t rwlock;
    atomic_t open_count;
    atomic_t entries;
    struct class *device_class;
    struct device *device;
    int major_number;
} klog_t;

/* Global instance of the logger */
static struct klogger klog;

/* Function prototypes */
static int dev_open(struct inode *inodep, struct file *filep);
static int dev_release(struct inode *inodep, struct file *filep);
static ssize_t dev_read(struct file *filep, char __user *user_buffer, size_t count, loff_t *file_pos);
static ssize_t dev_write(struct file *filep, const char __user *user_buffer, size_t count, loff_t *file_pos);

/* File operations structure */
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

/**
 * dev_open() - Called when a process opens the device
 * @inodep: Pointer to the inode object
 * @filep: Pointer to the file object
 *
 * Increments the open_count to track number of processes using the device.
 *
 * Return: 0 on success, negative error code on failure
 */
static int dev_open(struct inode *inodep, struct file *filep) {
    // Check for potential overflow before incrementing
    if (atomic_read(&klog.open_count) == INT_MAX) {
        printk(KERN_ERR "klogger: Too many open handles\n");
        return -EMFILE;
    }
    atomic_inc(&klog.open_count);
    return 0;
}

/**
 * dev_release() - Called when a process closes the device
 * @inodep: Pointer to the inode object
 * @filep: Pointer to the file object
 *
 * Decrements the open_count when a process is done with the device.
 *
 * Return: 0 on success, negative error code on failure
 */
static int dev_release(struct inode *inodep, struct file *filep) {
    // Check for underflow before decrementing
    if (atomic_read(&klog.open_count) <= 0) {
        printk(KERN_WARNING "klogger: Device close called but no open handles\n");
        return -EINVAL;
    }
    atomic_dec(&klog.open_count);
    return 0;
}

/**
 * dev_read() - Read messages from the circular buffer
 * @filep: Pointer to the file object
 * @user_buffer: Buffer in user space to store read data
 * @count: Number of bytes to read
 * @file_pos: Current position in file
 *
 * Reads messages from the circular buffer starting at the tail position.
 * Uses read lock to ensure thread safety during read operations.
 *
 * Return: Number of bytes read, or negative error code on failure
 */
static ssize_t dev_read(struct file *filep, char __user *user_buffer, size_t count, loff_t *file_pos) {
    size_t bytes_read = 0;
    size_t bytes_to_copy;
    size_t current_pos = klog.tail;
    size_t entries_read = 0;
    char *buffer;

    if (!user_buffer || count == 0) {
        return -EINVAL;
    }

    if (*file_pos >= LOG_BUF_LEN) {
        return 0;
    }

    // Allocate temporary buffer - limit to count size
    buffer = kmalloc(min_t(size_t, count, LOG_BUF_LEN), GFP_KERNEL);
    if (!buffer) {
        return -ENOMEM;
    }

    read_lock(&klog.rwlock);
    // Read until we fill the temp buffer or reach the head
    while (bytes_read < count && entries_read < atomic_read(&klog.entries)) {

        // Get length of current message
        bytes_to_copy = strnlen(klog.log_buffer + (current_pos * MSG_LEN), MSG_LEN);
        
        // Don't copy more than the user requested
        if (bytes_read + bytes_to_copy > count) {
            bytes_to_copy = count - bytes_read;
        }

        // Copy to temp buffer
        memcpy(buffer + bytes_read,
               klog.log_buffer + (current_pos * MSG_LEN),
               bytes_to_copy);

        bytes_read += bytes_to_copy;
        entries_read++;

        // Stop if we've reached the head
        if (current_pos == klog.prev_head) {
            break;
        }

        // Move to next entry
        current_pos = (current_pos + 1) & (MAX_ENTRIES - 1);
    }

    read_unlock(&klog.rwlock);

    if (*file_pos >= bytes_to_copy) {
        return 0;
    }

    // Now copy all data to user space at once
    if (copy_to_user(user_buffer, buffer, bytes_read)) {
        kfree(buffer);
        return -EFAULT;
    }

    // Free the temporary buffer
    kfree(buffer);

    // Update file position with actual bytes read
    *file_pos += bytes_read;

    return bytes_read;
}

/**
 * dev_write() - Write a message to the circular buffer
 * @filep: Pointer to the file object
 * @user_buffer: Buffer in user space containing data to write
 * @count: Number of bytes to write
 * @file_pos: Current position in file
 *
 * Writes a message to the circular buffer at the head position.
 * If buffer is full, overwrites oldest message.
 * Uses write lock to ensure thread safety during write operations.
 *
 * Return: Number of bytes written, or negative error code on failure
 */
static ssize_t dev_write(struct file *filep, const char __user *user_buffer, size_t count, loff_t *file_pos) {
    size_t bytes_to_copy = count;
    size_t usr_idx = 0;

    // If incoming data is larger than the buffer, truncate to keep only the latest part
    if (count >= MSG_LEN) {
        usr_idx = count - (MSG_LEN - 1);
        bytes_to_copy = MSG_LEN - 1;
    }

    if (*file_pos >= MSG_LEN) {
        return 0;
    }

    write_lock(&klog.rwlock);

    if (atomic_read(&klog.entries) == MAX_ENTRIES && klog.head == klog.tail) {
        klog.tail = (klog.tail + 1) & (MAX_ENTRIES - 1);
    }

    if (copy_from_user(klog.log_buffer + (klog.head * MSG_LEN), user_buffer + usr_idx, bytes_to_copy)) {
        write_unlock(&klog.rwlock);
        return -EFAULT;
    }

    klog.log_buffer[(klog.head * MSG_LEN) + bytes_to_copy] = '\0';

    if (atomic_read(&klog.entries) < MAX_ENTRIES) {
        atomic_inc(&klog.entries);
    }

    klog.prev_head = klog.head;
    klog.head = (klog.head + 1) & (MAX_ENTRIES - 1);
    
    write_unlock(&klog.rwlock);  // Unlock after writing

    return count; // Return number of bytes written
}

/**
 * klogger_init() - Initialize the kernel logger module
 *
 * Initializes the circular buffer, synchronization primitives,
 * and creates the character device.
 *
 * Return: 0 on success, negative error code on failure
 */
static int __init klogger_init(void) {

    // Initialize the device structure
    // klog.log_buffer = kmalloc(LOG_BUF_LEN, GFP_KERNEL);
    // if (!klog.log_buffer) {
    //     printk(KERN_ERR "Failed to allocate ring buffer.\n");
    //     return -ENOMEM;
    // }
    memset(klog.log_buffer, 0, LOG_BUF_LEN);
    klog.head = 0;
    klog.tail = 0;
    klog.prev_head = 0;

    // Initialize synchronization primitivesklog.log_buffer = kmalloc(LOG_BUF_LEN, GFP_KERNEL);
    // if (!klog.log_buffer) {
    //     printk(KERN_ERR "Failed to allocate ring buffer.\n");
    //     return -ENOMEM;
    // }
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

/**
 * klogger_exit() - Cleanup and unregister the kernel logger module
 *
 * Destroys the character device, class, and frees resources.
 * Warns if there are still open handles to the device.
 */
static void __exit klogger_exit(void) {

    // Free buffer
    // kfree(klog.log_buffer);

    // if there are still handles open print a message
    if (atomic_read(&klog.open_count) != 0) {
        printk(KERN_WARNING "There are still %d device(s) open.\n", atomic_read(&klog.open_count));
    }

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

