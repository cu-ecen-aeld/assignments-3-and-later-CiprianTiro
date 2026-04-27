#include "systemcalls.h"
#include "stdlib.h"
#include "unistd.h"
#include <sys/wait.h>
#include "fcntl.h"


/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    if ( system(cmd) != 0 )
    {
        return false;
    }
    else
    {
        return true;
    }
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    pid_t child;

    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    va_end(args);

    /* Check if command[0] is absolute. */
    if (command[0][0] != '/')
        return false;

    fflush(stdout);
    /* Create new child process. */
    child = fork();

    if ( child == -1 )
    {
        /* Fork fail. */
        return false;
    }
    else if ( child == 0)
    {
        /* Child process. */
        execv(command[0], command);

        /* Reaching this line, means that execv has failed. */
        perror("execv failed");
        _exit(EXIT_FAILURE);
    }
    else 
    {
        int status;
        /* child > 0 --> Parent process. */
        /* Wait for child process to finish executing commands. */
        if ( waitpid(child, &status, 0) == -1)
        {
            /* Error on wait. */
            perror("wait() error");
            return false;
        }

        /* Check if the child actually reported an error! */
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            /* Only return true if exit code is 0. */
            return true; 
        } else
        {
            /* Return false if execv failed (exit code 1). */
            return false;
        }
    }
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    pid_t child;

    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    va_end(args);

    /* Check if command[0] is absolute. */
    if (command[0][0] != '/')
        return false;

    fflush(stdout);
    /* Create new child process. */
    child = fork();

    if ( child == -1 )
    {
        /* Fork fail. */
        return false;
    }
    else if ( child == 0)
    {
        /* Child process. */

        /* Writing to the file should be done by child process. */
        int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
        if ( fd < 0 )
        { 
            perror("Error on file open.");
            _exit(EXIT_FAILURE);
        }
        
        /* Duplicate fd and then redirect the STDOUT to the file. */
        if ( dup2(fd, STDOUT_FILENO) == -1)
        {
            perror("dup2");
            _exit(EXIT_FAILURE);
        }
        /* Close the additional not needed file descriptor. */
        close(fd);
        
        /* Execute command. */
        execv(command[0], command);

        /* Reaching this line, means that execv has failed. */
        perror("execv failed");

        _exit(EXIT_FAILURE);
    }
    else 
    {
        int status;
        /* child > 0 --> Parent process. */
        /* Wait for child process to finish executing commands. */
        if ( waitpid(child, &status, 0) == -1)
        {
            /* Error on wait. */
            perror("wait() error");
            return false;
        }

        /* Check if the child actually reported an error! */
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            /* Only return true if exit code is 0. */
            return true; 
        } else
        {
            /* Return false if execv failed (exit code 1). */
            return false;
        }
    }
}
