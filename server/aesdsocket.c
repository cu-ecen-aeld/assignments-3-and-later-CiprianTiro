#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/queue.h>
#include <time.h>

#ifndef STAILQ_FOREACH_SAFE
/**
 * STAILQ_FOREACH_SAFE - Iterates over a tail queue safely against removal of tail queue entry
 * @var:    The loop variable (current node)
 * @head:   The head of the tail queue
 * @field:  The name of the STAILQ_ENTRY field within the structure
 * @tvar:   A temporary pointer used to store the next node's address 
 *          before the current node is potentially freed.
 */
#define STAILQ_FOREACH_SAFE(var, head, field, tvar)           \
    for ((var) = STAILQ_FIRST((head));                        \
        (var) && ((tvar) = STAILQ_NEXT((var), field), 1);     \
        (var) = (tvar))
#endif

/* Global variables to be accessed by signal handler. */
int server_fd = -1;
int client_fd = -1;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Chosed usage of STAILQ (Singly-linked Tail Queue) because it can point to its head but also to the tail. */
typedef struct thread_node
{
    pthread_t thread_id;
    int client_fd;
    char *client_ip;
    int is_complete;
    STAILQ_ENTRY(thread_node) entries;
} thread_node_t;

/* Define the head of the list. */
STAILQ_HEAD(thread_list, thread_node);

/* sig atomic is required because writing to an integer takes multiple instructions and a signal handler might interrupt the write operation. */
volatile sig_atomic_t appRunning = 1;

void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
    {
        syslog(LOG_DEBUG, "Caught signal, exiting");

        appRunning = 0;

        /* Shutdown forces accept() to return so the main loop can exit */
        if (server_fd != -1)
        {
            shutdown(server_fd, SHUT_RDWR);
        }
    }
}

void setSignalAction()
{
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    /* No special flags. */
    sa.sa_flags = 0;
    /* No signals blocked during handler execution. */
    sigemptyset(&sa.sa_mask);
    /* Set the handlers for SIGINT and SIGTERM. */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

void* thread_handlerClientConnection(void *arg)
{
    /* Buffer to hold data from client. */
    char buffer[1024];
    ssize_t bytes_read;
    thread_node_t *node = (thread_node_t *)arg;
    
    int packet_complete = 0;
    
    syslog(LOG_INFO, "Accepted connection from %s", node->client_ip);
    
    /* Read data from the client until EOF. */
    while (!packet_complete && (bytes_read = recv(node->client_fd, buffer, sizeof(buffer), 0)) > 0)
    {
        /* Lock the file mutex to ensure thread safety even before the file is opened. */
        pthread_mutex_lock(&file_mutex);

        /* Open file for the duration of this client's session.
        Using a+ to avoid opening and closing the file multiple times. */
        FILE *file = fopen("/var/tmp/aesdsocketdata", "a+");
        if (file != NULL)
        {
            /* This usage of syslog ensures that only the bytes_read amount is printed, ignoring any "garbage" left over in the buffer from previous connections. */
            syslog(LOG_DEBUG, "Writing %.*s with %zd bytes to data file from client: %s", (int)bytes_read, buffer, bytes_read, node->client_ip);
            fwrite(buffer, 1, bytes_read, file);
    
            /* If the last character is a newline, send the file contents back to the client. */
            if (memchr(buffer, '\n', bytes_read) != NULL)
            {
                packet_complete = 1;
                /* Ensure data is physically on disk before we try to read it back. */
                fflush(file);
    
                /* Clear the EOF flag from previous reads and reset to start. */
                clearerr(file);
    
                /* Reset to beginning of file to send everything back. */
                fseek(file, 0, SEEK_SET);
    
                /* Read the file contents and send it back to the client. */
                char file_buffer[1024];
                size_t bytes_to_send;
                while ((bytes_to_send = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0)
                {
                    syslog(LOG_DEBUG, "Sending %.*s with %zu bytes back to client: %s", (int)bytes_to_send, file_buffer, bytes_to_send, node->client_ip);
                    if (send(node->client_fd, file_buffer, bytes_to_send, 0) < 0)
                    {
                        syslog(LOG_ERR, "Failed to send data back to client");
                        break;
                    }
                }
            }
            fclose(file);
        }
        pthread_mutex_unlock(&file_mutex);
    }

    syslog(LOG_INFO, "Closed connection from %s", node->client_ip);
    /* Close the client socket if it's still open. */
    close(node->client_fd);

    /* Mark the thread as complete so the main thread can reap (free) the memory. */
    node->is_complete = 1;

    return NULL;
}

void* timestamp_timer(void* arg)
{
    while (appRunning)
    {
        /* Wait for 10 seconds but only if the application is still running. */
        for (int i = 0; i < 10 && appRunning; i++)
        {
            sleep(1);
        }
        
        if (!appRunning) break;

        time_t rawtime;
        struct tm *info;
        char buffer[100];

        time(&rawtime);
        info = localtime(&rawtime);

        /* RFC 2822 format: %a, %d %b %Y %H:%M:%S %z
           Requirement: "timestamp:year, month, day, hour, minute, second"
           strftime format: %Y, %m, %d, %H, %M, %S*/
        strftime(buffer, sizeof(buffer), "timestamp:%Y, %m, %d, %H, %M, %S\n", info);

        pthread_mutex_lock(&file_mutex);
        FILE *file = fopen("/var/tmp/aesdsocketdata", "a+");
        if (file)
        {
            fputs(buffer, file);
            fclose(file);
        }
        pthread_mutex_unlock(&file_mutex);
    }
    return NULL;
}

int main (int argc, char *argv[])
{
    struct sockaddr_in address;
    /* Clean the memory. */
    memset(&address, 0, sizeof(address));

    /* Initialize the queue. */
    struct thread_list head;
    STAILQ_INIT(&head);
    /* Start the system logger. */
    openlog("aesdsocket", LOG_PID, LOG_USER);

    /* Setup the signal action. */
    setSignalAction();

    /* Create the socket.
    AF_INET: IPv4 protocol
    SOCK_STREAM: TCP protocol
    0: Use default protocol for TCP. */
    syslog(LOG_DEBUG, "Create socket");
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    /* Set the socket option to reuse address. */
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (server_fd < 0)
    {
        syslog(LOG_ERR, "Failed to create socket");
        closelog();
        exit(EXIT_FAILURE);
    }

    /* Set the socket address information. */
    /* AF_INET: IPv4 protocol
       INADDR_ANY: Listen on all available network interfaces
       htons(9000): Port 9000, converted to "Network Byte Order, else wrong port will be used" */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(9000);

    /* Bind the socket to the address. */
    syslog(LOG_DEBUG, "Bind socket to address %d", ntohs(address.sin_port));
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        syslog(LOG_ERR, "Failed to bind socket");
        close(server_fd);
        closelog();
        exit(EXIT_FAILURE);
    }

    /* After binding, check if the process should be a daemon. */
    if (argc > 1 && strcmp(argv[1], "-d") == 0)
    {   
        syslog(LOG_DEBUG, "Daemonizing process on port %d", ntohs(address.sin_port));
        pid_t pid = fork();
        if (pid < 0)
        {
            syslog(LOG_ERR, "Failed to fork process for daemon");
            close(server_fd);
            closelog();
            exit(EXIT_FAILURE);
        }
        else if (pid > 0)
        {
            /* Parent process can exit, child continues as daemon. */
            exit(0);
        }

        /* Create new session.
           The process is bonded to the terminal session, so we need 
           to create a new session which detaches it from the terminal. */
        setsid();
        /* Change working directory to root to avoid locking any directories. */
        chdir("/");
        /* Close standard file descriptors. */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    /* Listen for connections on the socket. */
    syslog(LOG_DEBUG, "Listen for connections on the socket");
    if (listen(server_fd, 10) < 0)
    {
        syslog(LOG_ERR, "Failed to listen on socket");
        close(server_fd);
        closelog();
        exit(EXIT_FAILURE);
    }
    
    pthread_t timer_thread;
    /* Create the timer thread. */
    if (pthread_create(&timer_thread, NULL, timestamp_timer, NULL) != 0)
    {
        syslog(LOG_ERR, "Failed to create timer thread");
        close(server_fd);
        closelog();
        exit(EXIT_FAILURE);
    }

    /* As long as there is no signal to exit. */
    while(appRunning)
    {
        struct sockaddr_in client_addr;
        /* Required for accept function. */
        socklen_t addr_len = sizeof(client_addr);
        /* Clean the memory. */
        memset(&client_addr, 0, sizeof(client_addr));
        
        syslog(LOG_DEBUG, "Waiting for a coonnection on the socket or signal to exit");
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0)
        {
            syslog(LOG_ERR, "Failed to accept connection");
            /* The signal handler will handle the actual exit logic. */
            continue;
        }

        /* Allocate a new thread node for the client connection. */
        thread_node_t *new_node = malloc(sizeof(thread_node_t));
        new_node->client_fd = client_fd;
        new_node->is_complete = 0;
        new_node->client_ip = strdup(inet_ntoa(client_addr.sin_addr));
        syslog(LOG_INFO, "Accepted connection from %s", new_node->client_ip);

        /* Add the new node to the list. */
        STAILQ_INSERT_TAIL(&head, new_node, entries);

        /* Create the thread for handling the client connection. */
        pthread_create(&new_node->thread_id, NULL, thread_handlerClientConnection, new_node);

        /* While running, check for completed threads, and clean them up. */
        thread_node_t *curr, *t_next;
        STAILQ_FOREACH_SAFE(curr, &head, entries, t_next)
        {
            if (curr->is_complete)
            {
                pthread_join(curr->thread_id, NULL);
                STAILQ_REMOVE(&head, curr, thread_node, entries);
                free(curr->client_ip);
                free(curr);
            }
        }
    }

    syslog(LOG_INFO, "Shutting down, cleaning up threads...");

    /* Wait for the timer thread to finish. */
    pthread_join(timer_thread, NULL);

    /* If signal received, then wait for all running threads to complete. */
    thread_node_t *curr, *t_next;
    STAILQ_FOREACH_SAFE(curr, &head, entries, t_next)
    {
        pthread_join(curr->thread_id, NULL);
        free(curr->client_ip);
        free(curr);
    }

    pthread_mutex_destroy(&file_mutex);

    /* Delete the data file if it exists. */
    syslog(LOG_DEBUG, "Deleting data file if it exists");
    unlink("/var/tmp/aesdsocketdata");
    
    /* Close the socket so the port is freed immediately. */
    if (server_fd != -1)
    {
        syslog(LOG_DEBUG, "Closing socket");
        close(server_fd);
    }
    
    syslog(LOG_DEBUG, "Shutdown complete, exiting");
    closelog();

    /* Gracefully exit. */
    return 0;
}