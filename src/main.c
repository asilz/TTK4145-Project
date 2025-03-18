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
    uint8_t index = 0;
    uint8_t is_backup = 0;

    while (1)
    {
        switch (getopt(argc, argv, "i:b:"))
        {
        case 'i':
            sscanf(optarg, "%" SCNu8, &index);
            break;
        case 'b':
            sscanf(optarg, "%" SCNu8, &is_backup);
        case -1:
            return process_init(!is_backup, index);
        }
    }
}