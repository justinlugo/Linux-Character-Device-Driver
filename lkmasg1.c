/**
 * File:	lkmasg1.c
 * Written for Linux 5.19 by: Justin Lugo, Mathew Reilly, Tiuna Benito Saenz
 * Adapted from code written by: John Aedo
 * Class:	COP4600-SP23
 */

#include <linux/module.h>	  // Core header for modules.
#include <linux/device.h>	  // Supports driver model.
#include <linux/kernel.h>	  // Kernel header for convenient functions.
#include <linux/fs.h>		  // File-system support.
#include <linux/uaccess.h>	  // User access copy function support.
#define DEVICE_NAME "lkmasg1" // Device name.
#define CLASS_NAME "char"	  ///< The device class -- this is a character device driver
#define BUFFER_SIZE 1024	  // Size of buffer; document says 1KB -> 1024 bytes

MODULE_LICENSE("GPL");						 ///< The license type -- this affects available functionality
MODULE_AUTHOR("Justin Lugo, Mathew Reilly, Tiuna Benito Saenz");					 ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("lkmasg1 Kernel Module"); ///< The description -- see modinfo
MODULE_VERSION("0.1");						 ///< A version number to inform users

/**
 * Important variables that store data and keep track of relevant information.
 */

static int major_number;
static int num_opens = 0;
static int error_count = 0;
static size_t message_length;
static char message[BUFFER_SIZE] = {0};

static struct class *lkmasg1Class = NULL;	///< The device-driver class struct pointer
static struct device *lkmasg1Device = NULL; ///< The device-driver device struct pointer

/**
 * Prototype functions for file operations.
 */
static int open(struct inode *, struct file *);
static int close(struct inode *, struct file *);
static ssize_t read(struct file *, char *, size_t, loff_t *);
static ssize_t write(struct file *, const char *, size_t, loff_t *);

/**
 * File operations structure and the functions it points to.
 */
static struct file_operations fops =
	{
		.owner = THIS_MODULE,
		.open = open,
		.release = close,
		.read = read,
		.write = write,
};

/**
 * Initializes module at installation
 */
int init_module(void)
{
	printk(KERN_INFO "lkmasg1: installing module.\n");

	// Allocate a major number for the device.
	major_number = register_chrdev(0, DEVICE_NAME, &fops);
	if (major_number < 0)
	{
		printk(KERN_ALERT "lkmasg1 could not register number.\n");
		return major_number;
	}
	printk(KERN_INFO "lkmasg1: registered correctly with major number %d\n", major_number);

	// Register the device class
	lkmasg1Class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(lkmasg1Class))
	{ // Check for error and clean up if there is
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "Failed to register device class\n");
		return PTR_ERR(lkmasg1Class); // Correct way to return an error on a pointer
	}
	printk(KERN_INFO "lkmasg1: device class registered correctly\n");

	// Register the device driver
	lkmasg1Device = device_create(lkmasg1Class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
	if (IS_ERR(lkmasg1Device))
	{								 // Clean up if there is an error
		class_destroy(lkmasg1Class); // Repeated code but the alternative is goto statements
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "Failed to create the device\n");
		return PTR_ERR(lkmasg1Device);
	}

	printk(KERN_INFO "lkmasg1: device class created correctly\n"); // Made it! device was initialized
	return 0;
}

/*
 * Removes module, sends appropriate message to kernel
 */
void cleanup_module(void)
{
	printk(KERN_INFO "lkmasg1: removing module.\n");
	device_destroy(lkmasg1Class, MKDEV(major_number, 0)); // remove the device
	class_unregister(lkmasg1Class);						  // unregister the device class
	class_destroy(lkmasg1Class);						  // remove the device class
	unregister_chrdev(major_number, DEVICE_NAME);		  // unregister the major number
	printk(KERN_INFO "lkmasg1: Goodbye from the LKM!\n");
	unregister_chrdev(major_number, DEVICE_NAME);
	return;
}

/*
 * Opens device module, sends appropriate message to kernel
 */
static int open(struct inode *inodep, struct file *filep)
{
	num_opens++;
	printk(KERN_INFO "lkmasg1: Device opened %d times successfully.\n", num_opens);
	return 0;
}

/*
 * Closes device module, sends appropriate message to kernel
 */
static int close(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "lkmasg1: Device closed successfully.\n");
	return 0;
}

/*
 * Reads from device, displays in userspace, and deletes the read data
 */
static ssize_t read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	// Setting length of buffer to that of message.
	if (len > message_length)
	{
		len = message_length;
	}
	
	// If we have a length of 0 (empty string) OR we have read our limit, return and send message to the LKM.
	if (message_length == 0)
	{
		printk(KERN_INFO "lkmasg1: No string read from device.\n");
		return (message_length == 0);
	}

	error_count = copy_to_user(buffer, message, len);
	message_length -= len;
	
	// We clear the buffer.	
	memset(message, '\0', message_length);
	
	// If there are no bits leftover, we can say it successfully read len amount of characters from the buffer.
	if (error_count == 0)
	{
		printk(KERN_INFO "lkmasg1: '%s' read from device (%zu characters).\n", buffer, len);
	}
	else
	{
		printk(KERN_INFO "lkmasg1: Failed to send %d characters to the user.\n", error_count);
	}
	return 0;
}

/*
 * Writes to the device
 */
static ssize_t write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	// If the length of our combined input strings is too long to write, we reduce the length to that of our write buffer.
	if (len > BUFFER_SIZE - message_length)
	{
		printk(KERN_INFO "lkmasg1: Input is now longer than buffer allows, reducing length to fit buffer.\n");
		len = BUFFER_SIZE - message_length;
	}
	
	// If we have a length of 0 (empty string) OR we are already at our buffer limit, return and send message to the LKM.
	if (len == 0)
	{
		printk(KERN_INFO "lkmasg1: No string written to device.\n");
		return (len == 0);
	}

	// Copy from buffer to message. Adding length of message to accomodate multiple writes.
	error_count = copy_from_user(message + message_length, buffer, len);
	message_length += len;
	
	// If there are no bits leftover, we can say it successfully wrote message_length amount of characters into the buffer.
	if (error_count == 0)
	{
		printk(KERN_INFO "lkmasg1: '%s' written to device (%zu characters).\n", message, message_length);
	}
	else
	{
		printk(KERN_INFO "lkmasg1: Failed to send %d characters to the user.\n", error_count);
	}
	return len;
}
