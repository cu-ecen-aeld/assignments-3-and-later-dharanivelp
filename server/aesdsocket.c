#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define BUFFER_LEN 256000
#define PORT_NUM 9000
#define FILE_PATH "/var/tmp/aesdsocketdata"

struct sockaddr_in server_addr;
int sockfd, filefd, client_addr_len;
pid_t pid;
bool exitflag = false;

void exit_func()
{
    if(exitflag == false)
    {
        syslog(LOG_INFO, "Caught signal, exiting");
        exitflag = true;
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        close(filefd);
    }
}

void handle_signal(int signal)
{
    if(signal == SIGINT || signal == SIGTERM)
    {
        exit_func();
    }
}

//handle data receiving and sending;
void *data_handle_func()
{
    struct sockaddr_in client_addr;
    struct stat sb;
    int filefd, client_sockfd, client_addr_len;
    ssize_t bytes_transferred;
    char *buffer;
    char client_ip[INET_ADDRSTRLEN];
    // register signal handler
    if(signal(SIGINT, handle_signal) == SIG_ERR || signal(SIGTERM, handle_signal) == SIG_ERR)
    {
        syslog(LOG_ERR, "unable to register the signal handler");
        exit_func();
    }

    filefd = open(FILE_PATH, O_RDWR|O_CREAT|O_APPEND|O_TRUNC, S_IRUSR|S_IWUSR);
    if(filefd == -1)
    {
        syslog(LOG_ERR, "Failed to open %s", FILE_PATH);
        exit_func();
    }

    buffer = (char *)malloc(BUFFER_LEN * sizeof(char));
    if(buffer == NULL)
    {
        exit_func();
    }
    while(!exitflag)
    {
        client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if(client_sockfd == -1)
        {
            exit_func();
        }
        inet_ntop(AF_INET, &client_addr, client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);
        if(client_sockfd == 0)
        {
            continue;
        }

        bytes_transferred = recv(client_sockfd, buffer, BUFFER_LEN, 0);
        if(bytes_transferred == -1)
        {
            syslog(LOG_ERR, "recv failed");
            exit_func();
        }
        if(bytes_transferred == 0)
        {
            continue;
        }

        write(filefd, buffer, bytes_transferred);
        fstat(filefd, &sb);
        char *filemap = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, filefd, 0);
        if(filemap == NULL)
        {
            syslog(LOG_ERR, "mmap error");
            exit_func();
        }
        bytes_transferred = send(client_sockfd, filemap, sb.st_size, 0);
        if(bytes_transferred == -1)
        {
            syslog(LOG_ERR, "failed to send data");
            exit_func();
        }
        munmap(filemap, sb.st_size);
        shutdown(client_sockfd, SHUT_RDWR);
        close(client_sockfd);
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
   }
   free(buffer);
}


int main(int argc, char **argv)
{
    openlog("aesdsocket", LOG_PID|LOG_ERR|LOG_CONS, LOG_USER);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)
    {
        syslog(LOG_ERR, "failed to create socket");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT_NUM);
    //set SO_REUSEADDR to avoid bind failure
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        syslog(LOG_ERR, "setsockopt failed");
        return -1;
    }
    //bind
    if(bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        syslog(LOG_ERR, "failed to bind");
        return -1;
    }
    //listen
    if(listen(sockfd, 5))
    {
        syslog(LOG_ERR, "failed to listed");
        return -1;
    }
    //run as daemon if "-d" is passed as argument orelse run as native
    if(argc > 1 && strcmp(argv[1], "-d") == 0)
    {
        pid = fork();
        if(pid != 0)
        printf("aesdsocket running as daemon %d\n", pid);
        if(pid == 0)
        {
            data_handle_func();
        }
    }
    else
    {
        printf("aesdsocket running as native\n");
        data_handle_func();
    }

    return 0;
}