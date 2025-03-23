#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define COUNT 4096

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		printf("Usage: %s <source> <destination>\n", argv[0]);
		exit(1);
	}

	const char* source_file 	 = argv[1];
	const char* destination_file = argv[2];

	int fd1 = open(source_file, O_RDONLY);
	if (fd1 == -1)
	{
		perror("Error while opening source file");
		exit(-1);
	}

	int fd2 = open(destination_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd2 == -1)
	{
		perror("Error while opening destination file");
		exit(-1);
	}

	char buf[COUNT];
	int n;
	while ((n = read(fd1, buf, sizeof(buf))) > 0)
	{
		if (write(fd2, buf, n) != n)
		{
			perror("Error while writing to destination file");
			exit(-2);
		}
	}

	if (n == -1)
	{
		perror("Error while reading from source file");
		exit(-3);
	}

	close(fd1);
	close(fd2);
	
	return 0;
}
