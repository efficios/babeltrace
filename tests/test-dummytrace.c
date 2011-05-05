#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <uuid/uuid.h>
#include <string.h>

#define FILE_LEN	(4 * 4096)

int main(int argc, char **argv)
{
	char *base, *pos;
	int fd, ret;
	off_t off;
	uuid_t uuid;

	fd = open("dummystream", O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
	if (fd < 0) {
		perror("open");
		return -1;
	}
	off = posix_fallocate(fd, 0, FILE_LEN);
	if (off < 0) {
		printf("Error in fallocate\n");
		return -1;
	}
#if 0
	{
		ssize_t len;

		off = lseek(fd, FILE_LEN - 1, SEEK_CUR);
		if (off < 0) {
			perror("lseek");
			return -1;
		}
		len = write(fd, "", 1);
		if (len < 0) {
			perror("write");
			return -1;
		}
	}
#endif
	base = mmap(NULL, FILE_LEN, PROT_READ|PROT_WRITE,
		MAP_SHARED, fd, 0);
	if (!base) {
		perror("mmap");
		return -1;
	}
	pos = base;

	/* magic */
	*(uint32_t *) pos = 0xC1FC1FC1;
	pos += sizeof(uint32_t);

	/* trace_uuid */
	ret = uuid_parse("2a6422d0-6cee-11e0-8c08-cb07d7b3a564", uuid);
	if (ret) {
		printf("uuid parse error\n");
		return -1;
	}
	memcpy(pos, uuid, 16);
	pos += 16;

	/* stream_id */
	*(uint32_t *) pos = 0;
        pos += sizeof(uint32_t);

	ret = munmap(base, FILE_LEN);
	if (ret) {
		perror("munmap");
		return -1;
	}
	close(fd);
	return 0;
}


