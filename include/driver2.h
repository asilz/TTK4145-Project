#include <netinet/ip.h>
#include <sys/socket.h>

#ifndef FLOOR_COUNT
#define FLOOR_COUNT 4
#endif

#ifndef ELEVATOR_COUNT
#define ELEVATOR_COUNT 3
#endif

#define ENOFLOOR 41 // 41 is not an error code defined in the posix standard, so I will use it for my own error code

struct socket_vtable_t_;

typedef enum ButtonType
{
    BUTTON_TYPE_HALL_UP = 0,
    BUTTON_TYPE_HALL_DOWN,
    BUTTON_TYPE_CAB,
} ButtonType;

typedef struct socket_t
{
    int fd;
    struct sockaddr address;
} socket_t;

int elevator_init(socket_t *sock, const struct sockaddr_in *address);
int elevator_set_motor_direction_(socket_t *sock, int8_t direction);
int elevator_set_button_lamp_(socket_t *sock, uint8_t floor_state, uint8_t floor);
int elevator_set_floor_indicator_(socket_t *sock, uint8_t floor);
int elevator_set_door_open_lamp_(socket_t *sock, uint8_t value);
int elevator_get_button_signals_(socket_t *sock, uint8_t *floor_states);
int elevator_get_floor_sensor_signal_(socket_t *sock);
int elevator_get_obstruction_signal_(socket_t *sock);
int elevator_reload_config(socket_t *sock);