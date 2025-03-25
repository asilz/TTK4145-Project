#ifndef DRIVER_H
#define DRIVER_H

#include <netinet/ip.h>

#ifndef FLOOR_COUNT
#define FLOOR_COUNT 4
#endif

#ifndef ELEVATOR_COUNT
#define ELEVATOR_COUNT 3
#endif

#define ENOFLOOR 41 // 41 is not an error code defined in the posix standard, so I will use it for my own error code

enum button_type
{
    BUTTON_TYPE_HALL_UP = 0,
    BUTTON_TYPE_HALL_DOWN,
    BUTTON_TYPE_CAB,
};

enum motor_direction
{
    MOTOR_DIRECTION_DOWN = -1,
    MOTOR_DIRECTION_STOP = 0,
    MOTOR_DIRECTION_UP = 1
};

struct elevator_t;
typedef int socket_t;

int elevator_init(const struct sockaddr_in *address);
int elevator_set_motor_direction(socket_t sock, enum motor_direction direction);
int elevator_set_button_lamp(socket_t sock, uint8_t floor_state, uint8_t floor);
int elevator_set_floor_indicator(socket_t sock, uint8_t floor);
int elevator_set_door_open_lamp(socket_t sock, uint8_t value);
int elevator_get_button_signals(socket_t sock, uint8_t *floor_states);
int elevator_get_floor_sensor_signal(socket_t sock);
int elevator_get_obstruction_signal(socket_t sock);
int elevator_reload_config(socket_t sock);
int elevator_update_state(socket_t sock, struct elevator_t *elevator_state);

#endif