#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Global variables to be accessed by signal handler. */
int server_fd = -1;
int client_fd = -1;

void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
    {
        syslog(LOG_DEBUG, "Caught signal, exiting");

        /* Delete the data file if it exists. */
        syslog(LOG_DEBUG, "Deleting data file if it exists");
        unlink("/var/tmp/aesdsocketdata");
        
        /* Close the socket so the port is freed immediately. */
        if (server_fd != -1)
        {
            syslog(LOG_DEBUG, "Closing socket");
            close(server_fd);
        }
        
        syslog(LOG_DEBUG, "Closing syslog");
        closelog();

        /* Gracefully exit. */
        exit(0);
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

void handleClientConnection(int client_fd, char *client_ip)
{
    /* Buffer to hold data from client. */
    char buffer[1024];
    ssize_t bytes_read;
    
    /* Open file for the duration of this client's session.
       Using a+ to avoid opening and closing the file multiple times. */
    FILE *file = fopen("/var/tmp/aesdsocketdata", "a+");
    if (file == NULL)
    {
        syslog(LOG_ERR, "Failed to open data file for writing");
        return;
    }

    /* Read data from the client until EOF. */
    while ((bytes_read = recv(client_fd, buffer, sizeof(buffer), 0)) > 0)
    {
        /* This usage of syslog ensures that only the bytes_read amount is printed, ignoring any "garbage" left over in the buffer from previous connections. */
        syslog(LOG_DEBUG, "Writing %.*s with %zd bytes to data file from client: %s", (int)bytes_read, buffer, bytes_read, client_ip);
        fwrite(buffer, 1, bytes_read, file);

        /* Ensure data is physically on disk before we try to read it back. */
        fflush(file);

        /* If the last character is a newline, send the file contents back to the client. */
        if (memchr(buffer, '\n', bytes_read) != NULL)
        {
            /* Reset to beginning of file to send everything back. */
            fseek(file, 0, SEEK_SET);

            /* Read the file contents and send it back to the client. */
            char file_buffer[1024];
            size_t bytes_to_send;
            while ((bytes_to_send = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0)
            {
                syslog(LOG_DEBUG, "Sending %.*s with %zu bytes back to client: %s", (int)bytes_to_send, file_buffer, bytes_to_send, client_ip);
                ssize_t sent = send(client_fd, file_buffer, bytes_to_send, 0);
                if (sent < 0)
                {
                    syslog(LOG_ERR, "Failed to send data back to client");
                    return;
                }
            }

            /* After sending, seek back to the end so the next recv appends correctly. */
            fseek(file, 0, SEEK_END);
        }
    }
    fclose(file);

    if (bytes_read < 0)
    {
        syslog(LOG_ERR, "Failed to read data from client %s", client_ip);
    }
}

int main (int argc, char *argv[])
{
    struct sockaddr_in address;
    /* Clean the memory. */
    memset(&address, 0, sizeof(address));

    char *client_ip;

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
    
    while(1)
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
        else

        /* Log connection from client IP */
        client_ip = inet_ntoa(client_addr.sin_addr);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);
        
        /* Handle the client connection. */
        handleClientConnection(client_fd, client_ip);

        /* Close this specific client connection */
        close(client_fd);

        /* Mark the client file descriptor as invalid. */
        client_fd = -1;
        syslog(LOG_DEBUG, "Closed connection from %s", client_ip);
    }

    return 0;
}