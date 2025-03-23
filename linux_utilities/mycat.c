#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#define COUNT 1024

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		//! output error statement and exit
		printf("Usage: %s <filepath>\n", argv[0]);
		exit (-1);
	}

	char buf[COUNT]; 
	// this program is hardcoded to open and print only the content of the file "/etc/passwd"
	const char* filename=argv[1];

	int fd = open(filename, O_RDONLY);
	if (fd < 0)
	{
		perror("Error occured while opening file");
		exit (-2);
	}

	ssize_t num_bytes = 0;
	while ((num_bytes = read(fd, buf, COUNT)) > 0)
	{
		
		if (write(1, buf, num_bytes) < 0)
		{
			perror("Error occured while writing to stdout\n");
			exit(-3);
		}
	}

	if (close (fd) < 0)
	{
		perror("Error occured while closing file descriptor\n");
		exit(4);
	}
	
	return 0;
}

