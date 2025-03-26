#ifndef ELEVATOR_H
#define ELEVATOR_H

#include <driver.h>
#include <inttypes.h>

typedef struct elevator_t
{
    uint8_t state;
    uint8_t floor_states[FLOOR_COUNT];
    uint8_t locking_elevator[2][FLOOR_COUNT];
    uint8_t current_floor;
    uint8_t target_floor;
    uint8_t direction;
    uint8_t disabled;
} elevator_t;

typedef struct system_state
{
    elevator_t elevators[ELEVATOR_COUNT];
    socket_t elevator_socket;
    socket_t peer_socket;
} system_state_t;

/**
 * @brief Runs the elevator
 *
 * @param system sockets and the state of the elevators
 * @param ports array of ports with length equal to ELEVATOR_COUNT
 * @param index index of the elevator to run
 */
void elevator_run(system_state_t *system, const uint16_t *ports, const size_t index);

#endif