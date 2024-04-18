#ifndef _MESSAGE_SLOT_H_
#define _MESSAGE_SLOT_H_

#include <linux/ioctl.h>

#define MAJOR_NUM 240
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned int)
#define DEVICE_RANGE_NAME "message_slot"
#define BUF_LEN 128
#define SUCCESS 0

typedef struct channel {
	long channel_id;
	int msg_size;
	char message[BUF_LEN];
	struct channel *next;
} channel;

typedef struct channel_list {
	struct channel *head;
} channel_list;

typedef struct file_data {
	int minor_num;
	long working_channel_id;
	channel *working_channel;
} file_data;

#endif