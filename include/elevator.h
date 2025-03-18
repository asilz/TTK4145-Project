#ifndef ELEVATOR_H
#define ELEVATOR_H

#include <driver.h>
#include <inttypes.h>

enum elevator_state_t
{
    ELEVATOR_STATE_IDLE = 0,
    ELEVATOR_STATE_MOVING = 1,
    ELEVATOR_STATE_OPEN = 2
};

typedef struct elevator_t
{
    enum elevator_state_t state;
    uint8_t floor_states[FLOOR_COUNT];
    uint8_t locking_elevator[FLOOR_COUNT];
    uint8_t current_floor;
    uint8_t target_floor;
} elevator_t;

typedef struct system_state
{
    elevator_t elevators[ELEVATOR_COUNT];
    socket_t elevator_socket;
    socket_t peer_socket;
} system_state_t;

void elevator_run(system_state_t *system, uint8_t index);

#endif