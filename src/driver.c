#include <driver.h>
#include <elevator.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

typedef struct packet
{
    int8_t command;
    int8_t args[3];
} packet_t;

enum command_type
{
    COMMAND_TYPE_RELOAD_CONFIG = 0,
    COMMAND_TYPE_MOTOR_DIRECTION,
    COMMAND_TYPE_ORDER_BUTTON_LIGHT,
    COMMAND_TYPE_FLOOR_INDICATOR,
    COMMAND_TYPE_DOOR_OPEN_LIGHT,
    COMMAND_TYPE_STOP_BUTTON_LIGHT,
    COMMAND_TYPE_ORDER_BUTTON,
    COMMAND_TYPE_FLOOR_SENSOR,
    COMMAND_TYPE_STOP_BUTTON,
    COMMAND_TYPE_OBSTRUCTION_SWITCH,
};

int driver_reload_config(socket_t sock)
{
    if (send(sock, &(packet_t){.command = COMMAND_TYPE_RELOAD_CONFIG}, sizeof(packet_t), MSG_NOSIGNAL) == -1)
    {
        return -errno;
    }
    return 0;
}

int driver_set_motor_direction(socket_t sock, motor_direction_t direction)
{
    if (send(sock, &(packet_t){.command = COMMAND_TYPE_MOTOR_DIRECTION, .args = {direction}}, sizeof(packet_t),
             MSG_NOSIGNAL) == -1)
    {
        return -errno;
    }
    return 0;
}

int driver_set_button_lamp(socket_t sock, uint8_t floor_state, uint8_t floor)
{

    for (uint8_t i = BUTTON_TYPE_HALL_UP; i <= BUTTON_TYPE_CAB; ++i)
    {
        if (send(sock,
                 &(packet_t){.command = COMMAND_TYPE_ORDER_BUTTON_LIGHT,
                             .args = {i, floor, (floor_state & (1 << i)) != 0}},
                 sizeof(packet_t), MSG_NOSIGNAL) == -1)
        {
            return -errno;
        }
    }
    return 0;
}

int driver_set_floor_indicator(socket_t sock, uint8_t floor)
{
    if (send(sock, &(packet_t){.command = COMMAND_TYPE_FLOOR_INDICATOR, .args = {floor}}, sizeof(packet_t),
             MSG_NOSIGNAL) == -1)
    {
        return -errno;
    }
    return 0;
}

int driver_set_door_open_lamp(socket_t sock, uint8_t value)
{
    if (send(sock, &(packet_t){.command = COMMAND_TYPE_DOOR_OPEN_LIGHT, .args = {value}}, sizeof(packet_t),
             MSG_NOSIGNAL) == -1)
    {
        return -errno;
    }
    return 0;
}

int driver_get_button_signals(socket_t sock, uint8_t *floor_states)
{
    for (uint8_t i = 0; i < FLOOR_COUNT; ++i)
    {
        for (uint8_t j = 0; j <= BUTTON_TYPE_CAB; ++j)
        {
            packet_t msg = {.command = COMMAND_TYPE_ORDER_BUTTON, .args = {j, i}};
            send(sock, &msg, sizeof(packet_t), MSG_NOSIGNAL);
            recv(sock, &msg, sizeof(packet_t), MSG_NOSIGNAL);
            floor_states[i] |= msg.args[0] << j;
        }
    }
    return 0;
}

int driver_get_floor_sensor_signal(socket_t sock)
{
    packet_t msg = {.command = COMMAND_TYPE_FLOOR_SENSOR};
    send(sock, &msg, sizeof(packet_t), MSG_NOSIGNAL);
    recv(sock, &msg, sizeof(packet_t), MSG_NOSIGNAL);
    if (msg.args[0])
    {
        return msg.args[1];
    }
    return -ENOFLOOR;
}

int driver_get_obstruction_signal(socket_t sock)
{
    packet_t msg = {.command = COMMAND_TYPE_OBSTRUCTION_SWITCH};
    send(sock, &msg, sizeof(msg), MSG_NOSIGNAL);
    recv(sock, &msg, sizeof(msg), MSG_NOSIGNAL);
    return msg.args[0];
}

socket_t driver_init(const struct sockaddr_in *address)
{
    socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1)
    {
        return -errno;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &(struct timeval){.tv_sec = 0, .tv_usec = 0},
                   sizeof(struct timeval)) == -1)
    {
        int err = -errno;
        (void)close(sock);
        return err;
    }

    if (connect(sock, (struct sockaddr *)address, sizeof(*address)) == -1)
    {
        int err = -errno;
        (void)close(sock);
        return err;
    }

    return sock;
}