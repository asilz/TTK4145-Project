#include <netinet/ip.h>
#include <sys/socket.h>

#ifndef FLOOR_COUNT
#define FLOOR_COUNT 4
#endif

#define ENOFLOOR 41 // 41 is not an error code defined in the posix standard, so I will use it for my own error code

struct socket_vtable_t_;

typedef enum ButtonType
{
    BUTTON_TYPE_HALL_UP = 0,
    BUTTON_TYPE_HALL_DOWN,
    BUTTON_TYPE_CAB,
} ButtonType;

typedef enum CommandType
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
} CommandType;

struct Message
{
    int8_t command;
    int8_t args[3];
};

typedef struct socket_t
{
    const struct socket_vtable_t_ *vfptr;
    struct sockaddr address;
    int fd;

} socket_t;

struct socket_vtable_t_
{
    int (*set_motor_direction)(socket_t *sock, int8_t direction);
    int (*set_button_lamp)(socket_t *sock, ButtonType buttonType, uint8_t floor, uint8_t value);
    int (*set_floor_indicator)(socket_t *sock, uint8_t floor);
    int (*set_door_open_lamp)(socket_t *sock, uint8_t direction);
    int (*get_button_signals)(socket_t *sock, ButtonType buttonType, uint8_t *floor_states);
    int (*get_floor_sensor_signal)(socket_t *sock);
    int (*get_obstruction_signal)(socket_t *sock);
};

int slave_init(socket_t *sock, struct sockaddr_in *address, const struct sockaddr_in *bind_address);
int elevator_init(socket_t *sock, const struct sockaddr_in *address);