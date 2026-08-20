#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define DEVICE_PATH "/dev/kcalc_chardev"
#define READ_BUF_SIZE 64

int main(void)
{
	int fd;
	int exit_code = 0;
	char *line = NULL;
	size_t buf_size = 0;
	ssize_t nread;

	fd = open(DEVICE_PATH, O_RDWR);
	if (fd < 0) {
		perror("open " DEVICE_PATH);
		return 1;
	}

	fputs("kcalc>>>", stdout);
	fflush(stdout);
	while ((nread = getline(&line, &buf_size, stdin)) != -1) {
		char read_buf[READ_BUF_SIZE];
		if (nread > 0 && line[nread - 1] == '\n') {
			line[nread - 1] = '\0';
			nread--;
		}
		if (nread == 0)
			goto prompt;

		ssize_t written = write(fd, line, (size_t)nread);
		if (written < 0) {
			fprintf(stderr, "kcalc: %s: %s\n", line,
				strerror(errno));
			exit_code = 1;
			goto prompt;
		}

		ssize_t r = read(fd, read_buf, sizeof(read_buf) - 1);
		if (r < 0) {
			perror("read " DEVICE_PATH);
			exit_code = 1;
			goto prompt;
		}
		// r is the length of the read string
		read_buf[r] = '\0';

		fputs(read_buf, stdout);
		exit_code = 0;
prompt:
		fputs("kcalc>>>", stdout);
		fflush(stdout);
	}
	free(line);
	close(fd);
	return exit_code;
}
