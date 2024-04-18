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
	char *message = argv[3];
	int msg_size = strlen(message);
	unsigned long channel_id = atoi(argv[2]); // 0 for invaild arg, ioctl will handle

	if (argc != 4) {
		fprintf(stderr, "invalid number of arguments Error: %s\n", strerror(EINVAL));
		exit(1);
	}

	file_desc = open(argv[1], O_RDWR);
	if (file_desc < 0) {
		fprintf(stderr, "Can't open device file: %s\n Error: %s\n", argv[1], strerror(errno));
		exit(1);
	}

	ret_val = ioctl(file_desc, MSG_SLOT_CHANNEL, channel_id);
	if (ret_val != 0) {
		fprintf(stderr, "ioctl failed Error: %s\n", strerror(errno));
		exit(1);
	}
	ret_val = write(file_desc, message, msg_size);
	if (ret_val != msg_size) {
		fprintf(stderr, "write failed Error: %s\n", strerror(errno));
		exit(1);
	}

	close(file_desc);

	exit(0);
}