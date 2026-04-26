#include "stdio.h"
#include "stdint.h"
#include "syslog.h"

int main (int argc, char* argv[])
{
    /* Setup syslog for logging. */
    openlog("writer-app", LOG_PID, LOG_USER);

    /* Validate arguments. */
    if (argc != 3) {
        syslog(LOG_ERR, "Error: Invalid number of arguments. Expected 2, got %d", argc - 1);
        fprintf(stderr, "Usage: %s <file_path> <string_to_write>\n", argv[0]);
        closelog();
        return 1;
    }

    /* Declare variables. */
    char *file_path = argv[1];
    char *string_to_be_written = argv[2];
    FILE *fp = fopen(file_path, "w");

    /* Assume the directory is created by the caller, so no check for its existence. */

    /* Write the string to the file. */
    syslog(LOG_DEBUG, "Writing %s to %s", string_to_be_written, file_path);
    if (fputs(string_to_be_written, fp) == EOF)
    {
        syslog(LOG_ERR, "Error: Failed to write to file %s", file_path);
        fclose(fp);
        closelog();
        return 1;
    }

    /* Cleanup */
    fclose(fp);
    closelog();
    return 0;
}