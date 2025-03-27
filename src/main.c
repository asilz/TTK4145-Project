#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <log.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <process.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    size_t index = 0;
    uint8_t is_backup = 0;

    while (1)
    {
        /* Parse command-line arguments */
        switch (getopt(argc, argv, "i:b:"))
        {
        case 'i':
            /* Convert the input string to an unsigned long and store it in index */
            sscanf(optarg, "%lu", &index);
            break;
        case 'b':
             /* Convert the input string to an unsigned 8-bit int and store it in is_backup */
            sscanf(optarg, "%" SCNu8, &is_backup);
            break;
        case -1:
            return process_init(!is_backup, index);
        }
    }
}