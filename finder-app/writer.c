#include <stdio.h>
#include <unistd.h>
#include <syslog.h>

int main(int argc, char** argv)
{
	openlog("writer", LOG_PID|LOG_CONS, LOG_USER);

	if(argc < 3)
	{
		fprintf(stderr, "Error: Arguments missing\n");
		syslog(LOG_ERR, "Arguments missing");
		return 1;
	}
	
	FILE *fd = fopen(argv[1], "w");
	if(fd == NULL)
	{
		fprintf(stderr, "Error: unable to open the file\n");
		syslog(LOG_ERR, "unable to open the file");
		return -1;
	}

	fprintf(fd, "%s\n", argv[2]);
	syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);

	fclose(fd);
	
	return 0;
}
