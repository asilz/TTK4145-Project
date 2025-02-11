
#include <driver.h>
#include <errno.h>
#include <log.h>
#include <sys/time.h>

#ifndef ELEVATOR_COUNT
#define ELEVATOR_COUNT 1
#endif

#ifndef MASTER_PORT
#define MASTER_PORT 17532
#endif

#ifndef SLAVE_PORT
#define SLAVE_PORT 17533
#endif

typedef enum SlaveState
{
    SLAVE_STATE_CONNECTED = 0,
    SLAVE_STATE_DISCONNECTED = 1
} SlaveState;

int main(void)
{

    uint8_t cab_calls[FLOOR_COUNT];
    socket_t elevator_sock;
    int master_sock_fd;

    struct sockaddr_in address;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(MASTER_PORT);
    address.sin_family = AF_INET;

    struct sockaddr_in bind_address = {
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK), .sin_port = htons(SLAVE_PORT), .sin_family = AF_INET};

    master_sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (master_sock_fd == -1)
    {
        return -errno;
    }
    int value = 1;
    if (setsockopt(master_sock_fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) == -1)
    {
        int err = -errno;
        (void)shutdown(master_sock_fd, SHUT_RDWR);
        return err;
    }

    struct timeval time;
    time.tv_usec = 0;
    time.tv_sec = 0xfffffffe;
    if (setsockopt(master_sock_fd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) == -1)
    {
        int err = -errno;
        (void)shutdown(master_sock_fd, SHUT_RDWR);
        return err;
    }

    if (bind(master_sock_fd, (struct sockaddr *)&bind_address, sizeof(bind_address)) == -1)
    {
        int err = -errno;
        (void)shutdown(master_sock_fd, SHUT_RDWR);
        return err;
    }

    address.sin_port = htons(15657);
    elevator_init(&elevator_sock, &address);
    address.sin_port = htons(MASTER_PORT);

    SlaveState state = SLAVE_STATE_CONNECTED;

    while (1)
    {
        while (state == SLAVE_STATE_CONNECTED)
        {
            int err = 0;

            struct Message msg;

            do
            {
            } while (recvfrom(master_sock_fd, &msg, sizeof(msg), MSG_NOSIGNAL, NULL, NULL) == -1);
            err = send(elevator_sock.fd, &msg, sizeof(msg), MSG_NOSIGNAL);
            uint8_t floor = msg.args[1];
            if (msg.command > 5)
            {
                err = recv(elevator_sock.fd, &msg, sizeof(msg), MSG_NOSIGNAL);
            }
            if (msg.command == COMMAND_TYPE_ORDER_BUTTON && msg.args[0] == BUTTON_TYPE_CAB)
            {
                cab_calls[floor] = msg.args[0];
            }

            err = sendto(master_sock_fd, &msg, sizeof(msg), MSG_NOSIGNAL, (struct sockaddr *)&address, sizeof(address));
        }
        while (state == SLAVE_STATE_DISCONNECTED)
        {
        }
    }

    return 0;
}