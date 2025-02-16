
#include <driver.h>
#include <errno.h>
#include <log.h>
#include <string.h>
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
    time.tv_usec = 10000;
    time.tv_sec = 0;
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
        uint8_t floor_calls[FLOOR_COUNT] = {0};

        int err = 0;
        for (size_t i = 0; i < FLOOR_COUNT; ++i)
        {
            for (size_t j = 0; j < BUTTON_TYPE_MAX; ++j)
            {
                struct Message msg;
                err = send(elevator_sock.fd, &(struct Message){.command = COMMAND_TYPE_ORDER_BUTTON, .args = {j, i}},
                           sizeof(msg), MSG_NOSIGNAL);
                err = recv(elevator_sock.fd, &msg, sizeof(msg), MSG_NOSIGNAL);
                // floor_calls[i] = 0; // Asil: This has to be fixed when disconnected state is implemented
                floor_calls[i] |= msg.args[0] << j;
            }
        }
        if (floor_calls[0])
        {
            LOG_INFO("Hall call at floor 0\n");
        }
        if (state == SLAVE_STATE_CONNECTED)
        {

            struct Packet packet;
            do
            {
            } while (recvfrom(master_sock_fd, &packet, sizeof(packet), MSG_NOSIGNAL, NULL, NULL) == -1);
            if (packet.command == COMMAND_TYPE_FLOOR_SENSOR)
            {
                LOG_INFO("recv floor sensor command\n");
            }
            if (packet.command == COMMAND_TYPE_ORDER_BUTTON_ALL)
            {
                memcpy((uint8_t *)(&packet) + 1, floor_calls, sizeof(floor_calls));
                err = sendto(master_sock_fd, &packet, sizeof(packet), MSG_NOSIGNAL, (struct sockaddr *)&address,
                             sizeof(address));
                continue;
            }
            if (packet.command == COMMAND_TYPE_ORDER_BUTTON)
            {
                continue;
            }
            err = send(elevator_sock.fd, &packet, sizeof(struct Message), MSG_NOSIGNAL);
            // uint8_t floor = msg.args[1];
            if (packet.command > 5)
            {
                err = recv(elevator_sock.fd, &packet, sizeof(struct Message), MSG_NOSIGNAL);
            }

            err = sendto(master_sock_fd, &packet, sizeof(packet), MSG_NOSIGNAL, (struct sockaddr *)&address,
                         sizeof(address));
            if (err == -1)
            {
                LOG_INFO("slave sendto failed %d\n", errno);
            }
        }
        if (state == SLAVE_STATE_DISCONNECTED)
        {
        }
    }

    return 0;
}