#include "message_slot.h"

#include <fcntl.h>      /* open */
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char** argv)
{
	int file_desc;
	int ret_val;
	char buffer[BUF_LEN];
	unsigned long channel_id = atoi(argv[2]); // 0 for invaild arg, ioctl will handle

	if (argc != 3) {
		fprintf(stderr, "invalid number of arguments Error: %s\n", strerror(EINVAL));
		exit(1);
	}

	file_desc = open(argv[1], O_RDWR);
	if (file_desc < 0) {
		fprintf(stderr, "Can't open device file - %s\n Error: %s\n", DEVICE_RANGE_NAME, strerror(errno));
		exit(1);
	}

	ret_val = ioctl(file_desc, MSG_SLOT_CHANNEL, channel_id);
	if (ret_val != 0) {
		fprintf(stderr, "ioctl failed Error: %s\n", strerror(errno));
		exit(1);
	}
	ret_val = read(file_desc, buffer, BUF_LEN);
	if (ret_val <= 0) {
		fprintf(stderr, "read failed Error: %s\n", strerror(errno));
		exit(1);
	}
	if (write(1, buffer, ret_val) != ret_val) {
		fprintf(stderr, "print out message failed Error: %s\n", strerror(errno));
		exit(1);
	}

	close(file_desc);

	exit(0);
}