#include <driver.h>
#include <errno.h>
#include <inttypes.h>
#include <log.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

static int elevator_set_motor_direction_(socket_t *sock, int8_t direction)
{
    if (send(sock->fd, &(struct Message){.command = COMMAND_TYPE_MOTOR_DIRECTION, .args = {direction}},
             sizeof(struct Message), MSG_NOSIGNAL) == -1)
    {
        return -errno;
    }
    return 0;
}

static int elevator_set_button_lamp_(socket_t *sock, ButtonType buttonType, uint8_t floor, uint8_t value)
{
    if (send(sock->fd,
             &(struct Message){.command = COMMAND_TYPE_ORDER_BUTTON_LIGHT, .args = {buttonType, floor, value}},
             sizeof(struct Message), MSG_NOSIGNAL) == -1)
    {
        return -errno;
    }
    return 0;
}

static int elevator_set_floor_indicator_(socket_t *sock, uint8_t floor)
{
    if (send(sock->fd, &(struct Message){.command = COMMAND_TYPE_FLOOR_INDICATOR, .args = {floor}},
             sizeof(struct Message), MSG_NOSIGNAL) == -1)
    {
        return -errno;
    }
    return 0;
}

static int elevator_set_door_open_lamp_(socket_t *sock, uint8_t value)
{
    if (send(sock->fd, &(struct Message){.command = COMMAND_TYPE_DOOR_OPEN_LIGHT, .args = {value}},
             sizeof(struct Message), MSG_NOSIGNAL) == -1)
    {
        return -errno;
    }
    return 0;
}

static int elevator_get_button_signals_(socket_t *sock, uint8_t *floor_states)
{
    for (uint8_t i = 0; i < FLOOR_COUNT; ++i)
    {
        for (uint8_t j = 0; j < BUTTON_TYPE_MAX; ++j)
        {
            struct Message msg = {.command = COMMAND_TYPE_ORDER_BUTTON, .args = {j, i}};
            send(sock->fd, &msg, sizeof(msg), MSG_NOSIGNAL);
            recv(sock->fd, &msg, sizeof(msg), MSG_NOSIGNAL);
            floor_states[i] |= msg.args[0] << j;
        }
    }
    return 0;
}

static int elevator_get_floor_sensor_signal_(socket_t *sock)
{
    struct Message msg = {.command = COMMAND_TYPE_FLOOR_SENSOR};
    send(sock->fd, &msg, sizeof(msg), MSG_NOSIGNAL);
    recv(sock->fd, &msg, sizeof(msg), MSG_NOSIGNAL);
    if (msg.args[0])
    {
        return msg.args[1];
    }
    return -ENOFLOOR;
}

static int elevator_get_obstruction_signal_(socket_t *sock)
{
    struct Message msg = {.command = COMMAND_TYPE_OBSTRUCTION_SWITCH};
    send(sock->fd, &msg, sizeof(msg), MSG_NOSIGNAL);
    recv(sock->fd, &msg, sizeof(msg), MSG_NOSIGNAL);
    return msg.args[0];
}

static const struct socket_vtable_t_ elevator_vtable_ = {
    .get_button_signals = elevator_get_button_signals_,
    .get_floor_sensor_signal = elevator_get_floor_sensor_signal_,
    .get_obstruction_signal = elevator_get_obstruction_signal_,
    .set_button_lamp = elevator_set_button_lamp_,
    .set_door_open_lamp = elevator_set_door_open_lamp_,
    .set_floor_indicator = elevator_set_floor_indicator_,
    .set_motor_direction = elevator_set_motor_direction_,
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

static int slave_set_motor_direction_(socket_t *sock, int8_t direction)
{
    struct Packet msg;
    do
    {
        sendto(sock->fd, &(struct Packet){.command = COMMAND_TYPE_MOTOR_DIRECTION, .args = {direction}},
               sizeof(struct Packet), MSG_NOSIGNAL, &sock->address, sizeof(sock->address));
    } while (recvfrom(sock->fd, &msg, sizeof(struct Packet), MSG_NOSIGNAL, NULL, NULL) == -1 ||
             msg.command != COMMAND_TYPE_MOTOR_DIRECTION);
    return 0;
}

static int slave_set_button_lamp_(socket_t *sock, ButtonType buttonType, uint8_t floor, uint8_t value)
{
    struct Packet msg;
    do
    {
        sendto(sock->fd,
               &(struct Packet){.command = COMMAND_TYPE_ORDER_BUTTON_LIGHT, .args = {buttonType, floor, value}},
               sizeof(struct Packet), MSG_NOSIGNAL, &sock->address, sizeof(sock->address));
    } while (recvfrom(sock->fd, &msg, sizeof(struct Packet), MSG_NOSIGNAL, NULL, NULL) == -1 ||
             msg.command != COMMAND_TYPE_ORDER_BUTTON_LIGHT);
    return 0;
}

static int slave_set_floor_indicator_(socket_t *sock, uint8_t floor)
{
    struct Packet msg;
    do
    {
        sendto(sock->fd, &(struct Packet){.command = COMMAND_TYPE_FLOOR_INDICATOR, .args = {floor}},
               sizeof(struct Packet), MSG_NOSIGNAL, &sock->address, sizeof(sock->address));
    } while (recvfrom(sock->fd, &msg, sizeof(struct Packet), MSG_NOSIGNAL, NULL, NULL) == -1 ||
             msg.command != COMMAND_TYPE_FLOOR_INDICATOR);
    return 0;
}

static int slave_set_door_open_lamp_(socket_t *sock, uint8_t value)
{
    struct Packet msg;
    do
    {
        sendto(sock->fd, &(struct Packet){.command = COMMAND_TYPE_DOOR_OPEN_LIGHT, .args = {value}},
               sizeof(struct Packet), MSG_NOSIGNAL, &sock->address, sizeof(sock->address));
    } while (recvfrom(sock->fd, &msg, sizeof(struct Packet), MSG_NOSIGNAL, NULL, NULL) == -1 ||
             msg.command != COMMAND_TYPE_DOOR_OPEN_LIGHT);
    return 0;
}

static int slave_get_button_signals_(socket_t *sock, uint8_t *floor_states)
{
    struct Packet packet;
    do
    {
        sendto(sock->fd, &(struct Packet){.command = COMMAND_TYPE_ORDER_BUTTON_ALL}, sizeof(struct Packet),
               MSG_NOSIGNAL, &sock->address, sizeof(sock->address));
    } while (recvfrom(sock->fd, &packet, sizeof(packet), MSG_NOSIGNAL, NULL, NULL) == -1 ||
             packet.command != COMMAND_TYPE_ORDER_BUTTON_ALL);
    memcpy(floor_states, (uint8_t *)(&packet) + 1, FLOOR_COUNT);
    if (floor_states[0])
    {
        LOG_INFO("get button signals: call at floor 0\n");
    }
    return 0;
}

static int slave_get_floor_sensor_signal_(socket_t *sock)
{
    struct Packet msg;
    bool retry = false;
    do
    {
        if (retry)
        {
            LOG_INFO("Floor sensor retry %d\n", errno);
        }
        int err = sendto(sock->fd, &(struct Packet){.command = COMMAND_TYPE_FLOOR_SENSOR}, sizeof(struct Packet),
                         MSG_NOSIGNAL, &sock->address, sizeof(sock->address));
        if (err == -1)
        {
            LOG_INFO("get floor signals send to failed %d\n", errno);
        }
        retry = true;
    } while (recvfrom(sock->fd, &msg, sizeof(msg), MSG_NOSIGNAL, NULL, NULL) == -1 ||
             msg.command != COMMAND_TYPE_FLOOR_SENSOR);
    if (msg.args[0])
    {
        return msg.args[1];
    }
    return -ENOFLOOR;
}

static int slave_get_obstruction_signal_(socket_t *sock)
{
    struct Packet msg;
    do
    {
        sendto(sock->fd, &(struct Packet){.command = COMMAND_TYPE_OBSTRUCTION_SWITCH}, sizeof(struct Packet),
               MSG_NOSIGNAL, &sock->address, sizeof(sock->address));
    } while (recvfrom(sock->fd, &msg, sizeof(struct Packet), MSG_NOSIGNAL, NULL, NULL) == -1 ||
             msg.command != COMMAND_TYPE_OBSTRUCTION_SWITCH);
    return msg.args[0];
}

static const struct socket_vtable_t_ slave_vtable_ = {
    .get_button_signals = slave_get_button_signals_,
    .get_floor_sensor_signal = slave_get_floor_sensor_signal_,
    .get_obstruction_signal = slave_get_obstruction_signal_,
    .set_button_lamp = slave_set_button_lamp_,
    .set_door_open_lamp = slave_set_door_open_lamp_,
    .set_floor_indicator = slave_set_floor_indicator_,
    .set_motor_direction = slave_set_motor_direction_,
};

int slave_init(socket_t *sock, struct sockaddr_in *address, const struct sockaddr_in *bind_address)
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

    sock->vfptr = &slave_vtable_;

    return 0;
}