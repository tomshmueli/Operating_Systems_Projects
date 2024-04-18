#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/slab.h>

MODULE_LICENSE("GPL");

//Our custom definitions of IOCTL operations
#include "message_slot.h"

channel_list devices_data[257];

//================== DEVICE FUNCTIONS ===========================
static int device_open(struct inode* inode, struct file*  file) {
	int minor = iminor(inode);
	file_data *data;
	printk("Invoking device_open(%p) minor num is: %d\n", file,minor);
	data = kmalloc(sizeof(file_data), GFP_KERNEL);
	if (data==NULL) {
		printk("file data malloc failed device_open(%p)\n", file);
		return -ENOMEM;
	}
	// save minor number
	// working_channel_id = 0 means channel has been set
	data->minor_num = minor;
	data->working_channel_id = 0;
	file->private_data = (void*)data;

	return SUCCESS;
}

//---------------------------------------------------------------
static int device_release(struct inode* inode, struct file*  file) {
	printk("Invoking device_release(%p,%p)\n", inode, file);
	kfree(file->private_data);
	return SUCCESS;
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset) {
	int i,msg_size, fail_flag = 0;
	char *last_message;
	file_data *data = (file_data*)(file->private_data);
	printk("Invoking device_read(%p,%ld)\n",file, length);
	if (data->working_channel_id == 0 || buffer == NULL) {
		return -EINVAL;
	}
	last_message = data->working_channel->message;
	msg_size = data->working_channel->msg_size;
	if (msg_size==0) {
		return -EWOULDBLOCK;
	}
	if (length < msg_size) {
		return -ENOSPC;
	}
	// transfer message to user's buffer
	for (i = 0; i < msg_size; ++i) {
		if (put_user(last_message[i], buffer + i) != 0) {
			fail_flag = 1;
		}
	}
	if (fail_flag) {
		return -EIO;
	}
	// return the number of input characters used
	return i;
}

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset) {
	int i,fail_flag=0;
	char kbuffer[BUF_LEN];
	file_data *data = (file_data*)(file->private_data);
	printk("Invoking device_write(%p,%ld)\n", file, length);
	if ((data->working_channel_id == 0) || buffer == NULL){
		return -EINVAL;
	}
	if (length == 0 || length > BUF_LEN) {
		return -EMSGSIZE;
	}
	// transfer message from user's buffer
	for (i = 0; i < length && i < BUF_LEN; ++i) {
		if (get_user(kbuffer[i], &buffer[i]) != 0) {
			fail_flag = 1;
		}
	}
	if (fail_flag) {
		return -EIO;
	}
	// save message on success
	data->working_channel->msg_size = length;
	for (i = 0; i < length && i < BUF_LEN; ++i) {
		data->working_channel->message[i] = kbuffer[i];
	}
	// return the number of input characters used
	return i;
}

//----------------------------------------------------------------
static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param) {
	file_data *data = (file_data*)(file->private_data);
	channel *head = devices_data[data->minor_num].head;
	channel *temp;
	printk("Invoking ioctl: setting channel id to %ld\n", ioctl_param);
	if(ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0){
		return -EINVAL;
	}
	// find channel in list and set as active channel or add a new one if it does not exist
	if (head == NULL) {
		head = kmalloc(sizeof(channel), GFP_KERNEL);
		if (head==NULL) {
			printk("new channel malloc failed in ioctl");
			return -ENOMEM;
		}
		head->channel_id = ioctl_param;
		head->msg_size = 0;
		head->next = NULL;
		data->working_channel_id = ioctl_param;
		data->working_channel = head;
		devices_data[data->minor_num].head=head;
		return SUCCESS;
	}
	else {
		temp = head;
		while (temp->next != NULL) {
			if (temp->channel_id== ioctl_param) {
				data->working_channel_id = ioctl_param;
				data->working_channel = temp;
				return SUCCESS;
			}
			temp = temp->next;
		}
		if (temp->channel_id == ioctl_param) {
			data->working_channel_id = ioctl_param;
			data->working_channel = temp;
			return SUCCESS;
		}
		temp->next = kmalloc(sizeof(channel), GFP_KERNEL);
		if (head==NULL) {
			printk("new channel malloc failed in ioctl");
			return -ENOMEM;
		}
		temp->next->channel_id = ioctl_param;
		temp->next->msg_size = 0;
		temp->next->next = NULL;
		data->working_channel_id = ioctl_param;
		data->working_channel = temp->next;
		return SUCCESS;
	}
}

//==================== DEVICE SETUP =============================

// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations Fops =
 {
	.owner = THIS_MODULE,
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.unlocked_ioctl = device_ioctl,
	.release = device_release,
 };

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init simple_init(void)
{
	int rc = -1;
	int i;
	// Register driver capabilities. Obtain major num
	rc = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops);
	// Negative values signify an error
	if (rc < 0)
	{
		printk(KERN_ERR "%s registraion failed for  %d\n",DEVICE_RANGE_NAME, MAJOR_NUM);
		return rc;
	}
	// initiate list for every minor number possible
	for(i=0;i<257;i++) {
		devices_data[i].head = NULL;
	}
	printk("Registeration is successful. ");
	return SUCCESS;
}

//---------------------------------------------------------------
static void __exit simple_cleanup(void)
{
	// Unregister the device
	// Should always succeed
	channel *head;
	channel *temp;
	int i;
	// free every channel and every list created
	for(i=0;i<257;i++){
		head=devices_data[i].head;
		while (head != NULL)
		{
			temp = head;
			head = head->next;
			kfree(temp);
		}
	}
	unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//========================= END OF FILE =========================