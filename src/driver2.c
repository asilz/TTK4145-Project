#include <driver2.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

int elevator_set_motor_direction_(socket_t *sock, int8_t direction)
{
    if (send(sock->fd, &(packet_t){.command = COMMAND_TYPE_MOTOR_DIRECTION, .args = {direction}}, 4, MSG_NOSIGNAL) ==
        -1)
    {
        return -errno;
    }
    return 0;
}

int elevator_set_button_lamp_(socket_t *sock, ButtonType buttonType, uint8_t floor, uint8_t value)
{
    if (send(sock->fd, &(packet_t){.command = COMMAND_TYPE_ORDER_BUTTON_LIGHT, .args = {buttonType, floor, value}}, 4,
             MSG_NOSIGNAL) == -1)
    {
        return -errno;
    }
    return 0;
}

int elevator_set_floor_indicator_(socket_t *sock, uint8_t floor)
{
    if (send(sock->fd, &(packet_t){.command = COMMAND_TYPE_FLOOR_INDICATOR, .args = {floor}}, 4, MSG_NOSIGNAL) == -1)
    {
        return -errno;
    }
    return 0;
}

int elevator_set_door_open_lamp_(socket_t *sock, uint8_t value)
{
    if (send(sock->fd, &(packet_t){.command = COMMAND_TYPE_DOOR_OPEN_LIGHT, .args = {value}}, 4, MSG_NOSIGNAL) == -1)
    {
        return -errno;
    }
    return 0;
}

int elevator_get_button_signals_(socket_t *sock, uint8_t *floor_states)
{
    for (uint8_t i = 0; i < FLOOR_COUNT; ++i)
    {
        for (uint8_t j = 0; j <= BUTTON_TYPE_CAB; ++j)
        {
            packet_t msg = {.command = COMMAND_TYPE_ORDER_BUTTON, .args = {j, i}};
            send(sock->fd, &msg, 4, MSG_NOSIGNAL);
            recv(sock->fd, &msg, 4, MSG_NOSIGNAL);
            floor_states[i] |= msg.args[0] << j;
        }
    }
    return 0;
}

int elevator_get_floor_sensor_signal_(socket_t *sock)
{
    packet_t msg = {.command = COMMAND_TYPE_FLOOR_SENSOR};
    send(sock->fd, &msg, 4, MSG_NOSIGNAL);
    recv(sock->fd, &msg, 4, MSG_NOSIGNAL);
    if (msg.args[0])
    {
        return msg.args[1];
    }
    return -ENOFLOOR;
}

int elevator_get_obstruction_signal_(socket_t *sock)
{
    packet_t msg = {.command = COMMAND_TYPE_OBSTRUCTION_SWITCH};
    send(sock->fd, &msg, 4, MSG_NOSIGNAL);
    recv(sock->fd, &msg, 4, MSG_NOSIGNAL);
    return msg.args[0];
}

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

    if (connect(sock->fd, (struct sockaddr *)address, sizeof(*address)) == -1)
    {
        int err = -errno;
        (void)close(sock->fd);
        return err;
    }

    return 0;
}