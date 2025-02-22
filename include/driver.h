#ifndef DRIVER_H
#define DRIVER_H

#include <netinet/ip.h>
#include <sys/socket.h>

#ifndef FLOOR_COUNT
#define FLOOR_COUNT 4
#endif

#ifndef ELEVATOR_COUNT
#define ELEVATOR_COUNT 3
#endif

struct socket_vtable_t_;

typedef enum button_type_e
{
    BUTTON_TYPE_HALL_UP = 0,
    BUTTON_TYPE_HALL_DOWN,
    BUTTON_TYPE_CAB,
    BUTTON_TYPE_MAX,
} button_type_e;

typedef enum command_type_e
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
    COMMAND_TYPE_ORDER_BUTTON_LIGHT_ALL,
} command_type_e;

struct packet_t
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
        } obstruction_switch_data;
        struct
        {
            uint8_t floor_states[FLOOR_COUNT];
        } order_button_all_data;
        struct
        {
            uint8_t floor_lights[FLOOR_COUNT];
        } order_button_light_all_data;
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
    int (*send_recv)(socket_t *sock, struct packet_t *packet);
    int (*recv)(socket_t *sock, struct packet_t *packet);
    int (*send)(socket_t *sock, const struct packet_t *packet);
};

/**
 * @brief sends @p packet
 *
 * @param sock socket to send data through
 * @param packet packet to send
 * @return 0 on success, otherwise negative error code
 */
int socket_send(socket_t *sock, const struct packet_t *packet);

/**
 * @brief recieves from @p socket and stores the result in @p packet
 *
 * @param sock socket to recieve from
 * @param packet packet to store recieved data
 * @return 0 on success, otherwise negative error code
 */
int socket_recv(socket_t *sock, struct packet_t *packet);

/**
 * @brief sends @p packet and recieves the result in @p packet
 *
 * @param sock socket to send and recieve from
 * @param packet packet to send and recieve to
 * @return 0 on success, otherwise negative error code
 */
int socket_send_recv(socket_t *sock, struct packet_t *packet);

/**
 * @brief Initializes a socket for communicating with a slave with address @p address using udp
 *
 * @param sock socket to initialize
 * @param address slave address
 * @param bind_address this programs address
 * @return 0 on success, otherwise negative error code
 */
int node_udp_init(socket_t *sock, const struct sockaddr_in *address, const struct sockaddr_in *bind_address);

/**
 * @brief Initializes a socket for communicating with an elevator with address @p address using tcp
 *
 * @param sock socket to initialize
 * @param address elevator address
 * @return 0 on success, otherwise negative error code
 */
int elevator_init(socket_t *sock, const struct sockaddr_in *address);

#endif