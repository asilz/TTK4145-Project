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
    BUTTON_TYPE_MAX,
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

    COMMAND_TYPE_ORDER_BUTTON_ALL,
} CommandType;

struct Packet
{
    uint8_t command;
    union
    {
        struct
        {
            int8_t motor_direction;
        } motor_direction_data;
        struct
        {
            uint8_t button_type;
            uint8_t floor;
            uint8_t value;
        } order_button_light_data;
        struct
        {
            uint8_t floor;
        } floor_indicator_data;
        struct
        {
            uint8_t value;
        } door_open_light_data;
        struct
        {
            uint8_t value;
        } stop_button_light_data;
        struct
        {
            union
            {
                struct
                {
                    uint8_t button;
                    uint8_t floor;
                } instruction;
                struct
                {
                    uint8_t pressed;
                } output;
            };

        } order_button_data;
        struct
        {
            union
            {
                struct
                {
                    // Empty
                } instruction;
                struct
                {
                    uint8_t at_floor;
                    uint8_t floor;
                } output;
            };
        } floor_sensor_data;
        struct
        {
            union
            {
                struct
                {
                    // Empty
                } instruction;
                struct
                {
                    uint8_t pressed;
                } output;
            };
        } stop_button_data;
        struct
        {
            union
            {
                struct
                {
                    // Empty
                } instruction;
                struct
                {
                    uint8_t active;
                } output;
            };
        } obstruction_switch;
        struct
        {
            uint8_t floor_states[FLOOR_COUNT];
        } order_button_all_data;
    };
};

typedef struct socket_t
{
    const struct socket_vtable_t_ *vfptr;
    struct sockaddr address;
    int fd;

} socket_t;

struct socket_vtable_t_
{
    int (*send_recv)(socket_t *sock, struct Packet *packet);
    int (*recv)(socket_t *sock, struct Packet *packet);
    int (*send)(socket_t *sock, const struct Packet *packet);
};

int node_udp_init(socket_t *sock, const struct sockaddr_in *address, const struct sockaddr_in *bind_address);
int elevator_init(socket_t *sock, const struct sockaddr_in *address);