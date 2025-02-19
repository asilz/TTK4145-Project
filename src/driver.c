#include <driver.h>
#include <errno.h>
#include <inttypes.h>
#include <log.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

struct ElevatorMessage
{
    uint8_t command;
    uint8_t args[3];
};

static int elevator_send_recv_(socket_t *sock, struct Packet *packet)
{
    if (packet->command == COMMAND_TYPE_ORDER_BUTTON_ALL)
    {
        memset(packet->order_button_all_data.floor_states, 0, sizeof(packet->order_button_all_data.floor_states));
        for (size_t i = 0; i < FLOOR_COUNT; ++i)
        {
            for (size_t j = 0; j < BUTTON_TYPE_MAX; ++j)
            {
                struct ElevatorMessage msg;
                send(sock->fd, &(struct ElevatorMessage){.command = COMMAND_TYPE_ORDER_BUTTON, .args = {j, i}},
                     sizeof(msg), MSG_NOSIGNAL);
                recv(sock->fd, &msg, sizeof(msg), MSG_NOSIGNAL);
                // floor_calls[i] = 0; // Asil: This has to be fixed when disconnected state is implemented
                packet->order_button_all_data.floor_states[i] |= msg.args[0] << j;
            }
        }
        return 0;
    }
    if (send(sock->fd, packet, sizeof(struct ElevatorMessage), MSG_NOSIGNAL) == -1)
    {
        return -errno;
    }
    if (packet->command > 5 && recv(sock->fd, packet, sizeof(struct ElevatorMessage), MSG_NOSIGNAL) == -1)
    {
        return -errno;
    }
    return 0;
}

static const struct socket_vtable_t_ elevator_vtable_ = {
    .send_recv = elevator_send_recv_,
    .send = NULL,
    .recv = NULL,
};

int elevator_init(socket_t *sock, const struct sockaddr_in *address)
{
    sock->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock->fd == -1)
    {
        return -errno;
    }

    struct timeval time;
    time.tv_sec = UINT32_MAX;
    time.tv_usec = 0;

    if (setsockopt(sock->fd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) == -1)
    {
        int err = -errno;
        (void)close(sock->fd);
        return err;
    }

    sock->address = *(struct sockaddr *)address;
    if (connect(sock->fd, &sock->address, sizeof(sock->address)) == -1)
    {
        int err = -errno;
        (void)close(sock->fd);
        return err;
    }

    sock->vfptr = &elevator_vtable_;

    return 0;
}

static int node_udp_send_recv(socket_t *sock, struct Packet *packet)
{
    uint8_t command = packet->command;
    do
    {
        sendto(sock->fd, packet, sizeof(*packet), MSG_NOSIGNAL, &sock->address, sizeof(sock->address));
    } while (recvfrom(sock->fd, packet, sizeof(*packet), MSG_NOSIGNAL, NULL, NULL) == -1 || packet->command != command);
    return 0;
}

static int node_udp_send(socket_t *sock, const struct Packet *packet)
{
    if (sendto(sock->fd, packet, sizeof(*packet), MSG_NOSIGNAL, &sock->address, sizeof(sock->address)) == -1)
    {
        return -errno;
    }

    return 0;
}

static int node_udp_recv(socket_t *sock, struct Packet *packet)
{
    if (recvfrom(sock->fd, packet, sizeof(*packet), MSG_NOSIGNAL, NULL, NULL) == -1)
    {
        return -errno;
    }

    return 0;
}

static const struct socket_vtable_t_ node_udp_vtable_ = {
    .send_recv = node_udp_send_recv,
    .send = node_udp_send,
    .recv = node_udp_recv,
};

int node_udp_init(socket_t *sock, const struct sockaddr_in *address, const struct sockaddr_in *bind_address)
{
    sock->address = *(struct sockaddr *)address;
    sock->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock->fd == -1)
    {
        return -errno;
    }
    int value = 1;
    if (setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) == -1)
    {
        int err = -errno;
        (void)close(sock->fd);
        return err;
    }

    struct timeval time;
    time.tv_sec = 0;
    time.tv_usec = 10000;
    if (setsockopt(sock->fd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) == -1)
    {
        int err = -errno;
        (void)close(sock->fd);
        return err;
    }

    if (bind(sock->fd, (struct sockaddr *)bind_address, sizeof(*bind_address)) == -1)
    {
        int err = -errno;
        (void)close(sock->fd);
        return err;
    }

    sock->vfptr = &node_udp_vtable_;

    return 0;
}