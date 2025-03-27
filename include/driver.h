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

typedef enum
{
    BUTTON_TYPE_HALL_UP = 0,
    BUTTON_TYPE_HALL_DOWN,
    BUTTON_TYPE_CAB,
} button_type_t;

typedef enum
{
    MOTOR_DIRECTION_DOWN = -1,
    MOTOR_DIRECTION_STOP = 0,
    MOTOR_DIRECTION_UP = 1
} motor_direction_t;

typedef int socket_t;

/**
 * @brief Initializes a socket connected to an elevator
 *
 * @param address address of the elevator
 * @return socket
 */
socket_t driver_init(const struct sockaddr_in *address);

/**
 * @brief Sets the motor direction of an elevator to @p direction
 *
 * @param sock elevator socket
 * @param direction motor direction
 * @return error code
 * @retval 0 on success, otherwise negative error code
 */
int driver_set_motor_direction(socket_t sock, motor_direction_t direction);

/**
 * @brief Sets the button lamps according to @p floor_state at @p floor
 *
 * @param sock elevator socket
 * @param floor_state bitmap
 * @param floor floor to adjust the lamps
 * @return error code
 * @retval 0 on success, otherwise negative error code
 */
int driver_set_button_lamp(socket_t sock, uint8_t floor_state, uint8_t floor);

/**
 * @brief Sets the floor indicator to @p floor
 *
 * @param sock elevator socket
 * @param floor floor index
 * @return error code
 * @retval 0 on success, otherwise negative error code
 */
int driver_set_floor_indicator(socket_t sock, uint8_t floor);

/**
 * @brief Sets the door open lamp to @p value
 *
 * @param sock elevator socket
 * @param value either 1 for on or 0 for off
 * @return error code
 * @retval 0 on success, otherwise negative error code
 */
int driver_set_door_open_lamp(socket_t sock, uint8_t value);

/**
 * @brief Receives button signals and stores them in @p floor_states
 *
 * @param sock elevator socket
 * @param floor_states byte array with size equal to FLOOR_COUNT
 * @return error code
 * @retval 0 on success, otherwise negative error code
 */
int driver_get_button_signals(socket_t sock, uint8_t *floor_states);

/**
 * @brief Receives button signals and stores them in @p floor_states
 *
 * @param sock elevator socket
 * @return floor index or error code
 * @retval floor index, negative error code on failure
 */
int driver_get_floor_sensor_signal(socket_t sock);

/**
 * @brief Receives obstruction signal
 *
 * @param sock elevator socket
 * @return obstruction signal or error code
 * @retval 1 or 0 on success, negative error code on failure
 */
int driver_get_obstruction_signal(socket_t sock);

/**
 * @brief Reloads elevator config
 *
 * @param sock elevator socket
 * @return error code
 * @retval 0 on success, otherwise negative error code
 */
int driver_reload_config(socket_t sock);

#endif