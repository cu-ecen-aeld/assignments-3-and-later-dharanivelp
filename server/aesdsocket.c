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
#include <pthread.h>

#define BUFFER_LEN 256000
#define PORT_NUM 9000
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define TIMER_INTERVAL_SEC 10
#define TIME_BUFF_SIZE 256

typedef struct thread_list_t
{
    pthread_t thread;
    struct thread_list_t *next;
} thread_list_t;

pthread_mutex_t mutex;

struct sockaddr_in server_addr;
int sockfd, client_addr_len;
pid_t pid;
bool exitflag = false;
char client_ip[INET_ADDRSTRLEN];
thread_list_t *thread_list;
pthread_t timer_thread;

void exit_func()
{
    if(exitflag == false)
    {
        syslog(LOG_INFO, "Caught signal, exiting");
        exitflag = true;
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);

        while(thread_list != NULL)
        {
            thread_list_t *tmp = thread_list;
            thread_list = thread_list->next;
            pthread_join(tmp->thread,NULL);
            free(tmp);
        }
        pthread_join(timer_thread,NULL);
        pthread_mutex_destroy(&mutex);
    }
}

void handle_signal(int signal)
{
    if(signal == SIGINT || signal == SIGTERM)
    {
        exit_func();
    }
}

thread_list_t *thread_insert(thread_list_t *thread_list, pthread_t thread)
{
    thread_list_t *new_node = (thread_list_t*)malloc(sizeof(thread_list_t));
    thread_list_t *tmp = thread_list;
    new_node->thread = thread;
    new_node->next = NULL;
    if(new_node == NULL)
    {
        return thread_list;
    }
    if(thread_list == NULL)
    {
        return new_node;
    }
    while(tmp->next != NULL)
    {
        tmp = tmp->next;
    }
    tmp->next = new_node;
    return thread_list;
}

void *timestamp()
{
    time_t start_time = time(NULL);
    while (!exitflag)
    {
        usleep(100);
        time_t current_time = time(NULL);
        if (difftime(current_time, start_time) < TIMER_INTERVAL_SEC)
        {
            continue;
        }
        start_time = current_time;
        struct tm *tmp = localtime(&current_time);
        char buff[TIME_BUFF_SIZE];
        strftime(buff, TIME_BUFF_SIZE, "timestamp: %Y, %m, %d, %H, %M, %S\n", tmp);
        pthread_mutex_lock(&mutex);
        int filefd = open(FILE_PATH, O_RDWR|O_APPEND, S_IRUSR|S_IWUSR);
        if(filefd == -1)
        {
            syslog(LOG_ERR, "Failed to open %s", FILE_PATH);
            pthread_mutex_unlock(&mutex);
            exit_func();
        }
        write(filefd, buff, strlen(buff));
        close(filefd);
        pthread_mutex_unlock(&mutex);
    }
}

void *data_thread(void *clientfd)
{
    pthread_mutex_lock(&mutex);
    ssize_t bytes_transferred = 0;
    int *tmp = clientfd;
    int client_sockfd = *tmp;
    char *buffer = (char *)malloc(BUFFER_LEN * sizeof(char));
    if(buffer == NULL)
    {
        exit_func();
    }
    bytes_transferred = recv(client_sockfd, buffer, BUFFER_LEN, 0);
    if(bytes_transferred == -1)
    {
        syslog(LOG_ERR, "recv failed");
        pthread_mutex_unlock(&mutex);
        exit_func();
    }
    if(bytes_transferred == 0)
    {
        pthread_mutex_unlock(&mutex);
        pthread_exit(NULL);
    }
    int filefd = open(FILE_PATH, O_RDWR|O_APPEND, S_IRUSR|S_IWUSR);
    if(filefd == -1)
    {
        syslog(LOG_ERR, "Failed to open %s", FILE_PATH);
        pthread_mutex_unlock(&mutex);
        exit_func();
    }
    write(filefd, buffer, bytes_transferred);
    struct stat sb;
    fstat(filefd, &sb);
    char *filemap = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, filefd, 0);
    if(filemap == NULL)
    {
        syslog(LOG_ERR, "mmap error");
        pthread_mutex_unlock(&mutex);
        exit_func();
    }
    bytes_transferred = send(client_sockfd, filemap, sb.st_size, 0);
    if(bytes_transferred == -1)
    {
        syslog(LOG_ERR, "failed to send data");
        pthread_mutex_unlock(&mutex);
        exit_func();
    }
    munmap(filemap, sb.st_size);
    shutdown(client_sockfd, SHUT_RDWR);
    close(client_sockfd);
    close(filefd);
    syslog(LOG_INFO, "Closed connection from %s", client_ip);
    pthread_mutex_unlock(&mutex);
    free(tmp);
    free(buffer);
    pthread_exit(NULL);
}

//handle data receiving and sending;
void *data_handle_func()
{
    struct sockaddr_in client_addr;
    int client_addr_len = 0;
    int *client_sockfd = NULL;
    int filefd = 0;
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
    close(filefd);

    pthread_mutex_init(&mutex, NULL);
    pthread_create(&timer_thread, NULL, timestamp, NULL);
    while(!exitflag)
    {
        int client_addr_len = 0;
        client_sockfd = NULL;
        client_sockfd = (int*)malloc(sizeof(int));
        *client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if(*client_sockfd == -1)
        {
            exit_func();
        }
        if(*client_sockfd == 0)
        {
            continue;
        }
        pthread_mutex_lock(&mutex);
        inet_ntop(AF_INET, &client_addr, client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);
        pthread_mutex_unlock(&mutex);

        pthread_t thread;
        pthread_create(&thread, NULL, data_thread, (void*)client_sockfd);
        thread_list = thread_insert(thread_list, thread);

   }
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