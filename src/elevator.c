#include <elevator.h>
#include <errno.h>
#include <log.h>
#include <netinet/ip.h>
#include <process.h>
#include <stdbool.h>
#include <time.h>

#define ELEVATOR_DISCONNECTED_TIME_SEC (6)
#define DOOR_OPEN_TIME_SEC (3)
#define DISABLED_TIMEOUT (8)

typedef enum
{
    FLOOR_FLAG_BUTTON_UP = 1,
    FLOOR_FLAG_BUTTON_DOWN = 1 << 1,
    FLOOR_FLAG_BUTTON_CAB = 1 << 2,
    FLOOR_FLAG_LOCKED_UP = 1 << 3,
    FLOOR_FLAG_LOCKED_DOWN = 1 << 4
} floor_flags_t;

typedef enum
{
    ELEVATOR_STATE_IDLE = 0,
    ELEVATOR_STATE_MOVING = 1,
    ELEVATOR_STATE_OPEN = 2,
} elevator_state_t;

typedef enum
{
    ELEVATOR_DIRECTION_UP = 0,
    ELEVATOR_DIRECTION_DOWN = 1,
} elevator_direction_t;

static floor_flags_t direction_to_floor_flag_button_(elevator_direction_t direction)
{
    static const uint8_t table[2] = {FLOOR_FLAG_BUTTON_UP, FLOOR_FLAG_BUTTON_DOWN};
    return table[direction];
}

static floor_flags_t direction_to_floor_flag_locked_(elevator_direction_t direction)
{
    static const uint8_t table[2] = {FLOOR_FLAG_LOCKED_UP, FLOOR_FLAG_LOCKED_DOWN};
    return table[direction];
}

static void move_to_floor(socket_t elevator_socket)
{
    int err = driver_get_floor_sensor_signal(elevator_socket);
    if (err < 0)
    {
        driver_set_motor_direction(elevator_socket, MOTOR_DIRECTION_UP);
        err = driver_get_floor_sensor_signal(elevator_socket);
        while (err < 0)
        {
            err = driver_get_floor_sensor_signal(elevator_socket);
        }
        driver_set_motor_direction(elevator_socket, MOTOR_DIRECTION_STOP);
    }
}

static void startup(elevator_t *elevator, socket_t elevator_socket)
{
    driver_reload_config(elevator_socket);
    move_to_floor(elevator_socket);
    elevator->current_floor = driver_get_floor_sensor_signal(elevator_socket);
    driver_set_floor_indicator(elevator_socket, elevator->current_floor);

    for (size_t i = 0; i < FLOOR_COUNT; ++i)
    {
        driver_set_button_lamp(elevator_socket, elevator->floor_states[i], i);
    }
    elevator->state = ELEVATOR_STATE_IDLE;
}

static void register_orders(elevator_t *elevators, const struct timespec *elevator_times, const size_t index)
{
    for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
    {
        /* Iterates through all elevators, excludig itself and disconnected elevators */
        if (i == index || (elevator_times[i].tv_sec + ELEVATOR_DISCONNECTED_TIME_SEC < elevator_times[index].tv_sec))
        {
            continue;
        }
        for (size_t j = 0; j < FLOOR_COUNT; ++j)
        {
            /* Check for button-up updates */
            if (elevators[index].floor_states[j] & FLOOR_FLAG_BUTTON_UP)
            {
                if (elevators[index].floor_states[j] & FLOOR_FLAG_LOCKED_UP)
                {
                    if ((elevators[i].floor_states[j] & (FLOOR_FLAG_LOCKED_UP | FLOOR_FLAG_BUTTON_UP)) ==
                        0) // If order was completed by a different elevator
                    {
                        elevators[index].floor_states[j] &=
                            FLOOR_FLAG_BUTTON_CAB | FLOOR_FLAG_LOCKED_DOWN | FLOOR_FLAG_BUTTON_DOWN;
                    }

                    /* If both elevators have the floor locked, they need to ensure they agree on who takes the
                     * order */
                    if (elevators[i].floor_states[j] & FLOOR_FLAG_LOCKED_UP)
                    {
                        if (elevators[i].locking_elevator[0][j] < elevators[index].locking_elevator[0][j] ||
                            elevators[index].disabled ||
                            elevator_times[i].tv_sec + ELEVATOR_DISCONNECTED_TIME_SEC <
                                elevator_times[index].tv_sec) // Prioritize based on index
                        {
                            elevators[index].locking_elevator[0][j] = elevators[i].locking_elevator[0][j];
                        }
                        else
                        {
                            elevators[i].locking_elevator[0][j] = elevators[index].locking_elevator[0][j];
                        }
                    }
                }
                /* If our elevator is not locking, but the other elevator is locking. Locking is important to
                 * communicate, so that we agree that the elevator can take the call */
                else if (elevators[i].floor_states[j] & FLOOR_FLAG_LOCKED_UP)
                {
                    elevators[index].floor_states[j] |= FLOOR_FLAG_LOCKED_UP;
                    elevators[index].locking_elevator[0][j] = elevators[i].locking_elevator[0][j];
                }
            }
            /* In this case our elevator is not aware of any calls, but will update its state if any other elevators
             * have a call registered */
            else if ((elevators[i].floor_states[j] & FLOOR_FLAG_BUTTON_UP) &&
                     ((elevators[i].floor_states[j] & FLOOR_FLAG_LOCKED_UP) == 0))
            {
                elevators[index].floor_states[j] |= FLOOR_FLAG_BUTTON_UP;
            }

            /* Check for button down updates */
            if (elevators[index].floor_states[j] & FLOOR_FLAG_BUTTON_DOWN)
            {
                if (elevators[index].floor_states[j] & FLOOR_FLAG_LOCKED_DOWN)
                {
                    if ((elevators[i].floor_states[j] & (FLOOR_FLAG_LOCKED_DOWN | FLOOR_FLAG_BUTTON_DOWN)) ==
                        0) // If order was completed by a different elevator
                    {
                        elevators[index].floor_states[j] &=
                            FLOOR_FLAG_BUTTON_CAB | FLOOR_FLAG_LOCKED_UP | FLOOR_FLAG_BUTTON_UP;
                    }

                    /* If both elevators have the floor locked. They need to ensure they agree on who takes the
                     * order */
                    if (elevators[i].floor_states[j] & FLOOR_FLAG_LOCKED_DOWN)
                    {
                        if (elevators[i].locking_elevator[1][j] < elevators[index].locking_elevator[1][j] ||
                            elevators[index].disabled) // Prioritize based on index
                        {
                            elevators[index].locking_elevator[1][j] = elevators[i].locking_elevator[1][j];
                        }
                        else
                        {
                            elevators[i].locking_elevator[1][j] = elevators[index].locking_elevator[1][j];
                        }
                    }
                }
                /* If our elevator is not locking, but the other elevator is locking. Locking is important to
                 * communicate, so that we agree that the elevator can take the call */
                else if (elevators[i].floor_states[j] & FLOOR_FLAG_LOCKED_DOWN)
                {
                    elevators[index].floor_states[j] |= FLOOR_FLAG_LOCKED_DOWN;
                    elevators[index].locking_elevator[1][j] = elevators[i].locking_elevator[1][j];
                }
            }
            /* In this case our elevator is not aware of any calls, but will update its state if any other elevators
             * have a call registered */
            else if ((elevators[i].floor_states[j] & FLOOR_FLAG_BUTTON_DOWN) &&
                     ((elevators[i].floor_states[j] & FLOOR_FLAG_LOCKED_DOWN) == 0))
            {
                elevators[index].floor_states[j] |= FLOOR_FLAG_BUTTON_DOWN;
            }
        }
    }
}

static bool floor_is_locked(const elevator_t *elevators, const struct timespec *elevator_times, const size_t index)
{
    /* Check if the elevator is actively handling a request at this floor */
    if ((elevators[index].floor_states[elevators[index].current_floor] & FLOOR_FLAG_BUTTON_CAB) == 0 &&
        elevators[index].current_floor != elevators[index].target_floor)
    {
        for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
        {
            /* Skip disconnected elevators */
            if ((elevator_times[i].tv_sec + ELEVATOR_DISCONNECTED_TIME_SEC < elevator_times[index].tv_sec))
            {
                continue;
            }
            /* If any elevator does not have the floor locked in either direction, return false */
            if (((elevators[i].floor_states[elevators[index].current_floor] &
                  direction_to_floor_flag_locked_(elevators[index].direction)) == 0) ||
                elevators[i].locking_elevator[elevators[index].direction][elevators[index].current_floor] != index)
            {
                return false;
            }
        }
    }
    return true;
}

static void complete_order(elevator_t *elevator, socket_t elevator_socket, const size_t index)
{
    elevator->disabled = 0;
    driver_set_door_open_lamp(elevator_socket, 0);
    elevator->floor_states[elevator->current_floor] &= ~FLOOR_FLAG_BUTTON_CAB;
    /* If this elevator currently owns the lock on the floor in the current direction */
    if (elevator->locking_elevator[elevator->direction][elevator->current_floor] == index)
    {
        elevator->locking_elevator[elevator->direction][elevator->current_floor] = 255;
        elevator->floor_states[elevator->current_floor] &= ~direction_to_floor_flag_button_(elevator->direction) &
                                                           ~direction_to_floor_flag_locked_(elevator->direction);
    }
    driver_set_button_lamp(elevator_socket, elevator->floor_states[elevator->current_floor], elevator->current_floor);
}

static bool order_is_available(const elevator_t *elevators, const struct timespec *elevator_times,
                               const struct timespec *current_time, elevator_direction_t direction, uint8_t floor)
{
    for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
    {
        if ((elevator_times[i].tv_sec + ELEVATOR_DISCONNECTED_TIME_SEC < current_time->tv_sec))
        {
            continue;
        }
        /* If any active elevator has a button request for this floor in the given direction, or if any has already
         * locked it, then the order is not available */
        if ((elevators[i].floor_states[floor] & direction_to_floor_flag_button_(direction)) == 0 ||
            ((elevators[i].floor_states[floor] & direction_to_floor_flag_locked_(direction)) != 0))
        {
            return false;
        }
    }
    return true;
}

static bool verify_locked_floors(elevator_t *elevators, const struct timespec *elevator_times,
                                 elevator_direction_t direction, const size_t index)
{
    for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
    {
        if (((elevator_times[i].tv_sec + ELEVATOR_DISCONNECTED_TIME_SEC < elevator_times[index].tv_sec)) ||
            (elevators[i].disabled && elevators[index].target_floor != elevators[i].current_floor))
        {
            /* If a disconnected elevator was recorded as the lock holder, take over the lock */
            if (elevators[index].locking_elevator[direction][elevators[index].target_floor] == i)
            {
                elevators[index].locking_elevator[direction][elevators[index].target_floor] = index;
            }
            continue;
        }

        /* Elevators must agree on who owns the lock for the target floor */
        if (elevators[index].locking_elevator[direction][elevators[index].target_floor] !=
            elevators[i].locking_elevator[direction][elevators[index].target_floor])
        {
            return false;
        }
    }
    return true;
}

void elevator_run(system_state_t *system, const uint16_t *ports, const size_t index)
{
    struct timespec door_timer;
    struct timespec disable_timer;
    struct timespec elevator_times[ELEVATOR_COUNT];
    for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
    {
        clock_gettime(CLOCK_REALTIME, &elevator_times[i]);
    }

    /* Run elevator startup */
    startup(&system->elevators[index], system->elevator_socket);

    while (1) // Main control loop
    {
        uint8_t floor_states[FLOOR_COUNT] = {0};
        elevator_t previous_state = system->elevators[index];

        /* Poll locval button inputs */
        driver_get_button_signals(system->elevator_socket, floor_states);
        for (size_t i = 0; i < FLOOR_COUNT; ++i)
        {
            system->elevators[index].floor_states[i] |= floor_states[i];
            LOG_INFO("floor_state %zu = %u\n", i, system->elevators[index].floor_states[i]);
        }
        /* Update current floor from sensor */
        int floor_signal_err = driver_get_floor_sensor_signal(system->elevator_socket);
        if (floor_signal_err >= 0)
        {
            system->elevators[index].current_floor = floor_signal_err;
        }

        /* Broadcast local elevator state to all peers */
        for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
        {
            if (i == index)
            {
                continue;
            }
            struct sockaddr_in broadcast_addr = {
                .sin_family = AF_INET, .sin_port = htons(ports[i]), .sin_addr.s_addr = INADDR_BROADCAST};
            int err = sendto(system->peer_socket, &system->elevators[index], sizeof(*system->elevators), MSG_NOSIGNAL,
                             (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
            if (err == -1)
            {
                LOG_ERROR("broadcast error = %d\n", errno);
            }
        }

        LOG_INFO("index = %zu, current_floor = %" PRIu8 ",target_floor = %" PRIu8 ", current_state = %" PRIu8
                 ", elevator_direction = %" PRIu8 ", disabled = %" PRIu8 "\n",
                 index, system->elevators[index].current_floor, system->elevators[index].target_floor,
                 system->elevators[index].state, system->elevators[index].direction, system->elevators[index].disabled);

        /* Receive elevator states via UDP */
        do
        {
            elevator_t elevator;
            struct sockaddr_in addr_in;
            socklen_t addr_size = sizeof(addr_in);
            while (recvfrom(system->peer_socket, &elevator, sizeof(elevator), MSG_NOSIGNAL, (struct sockaddr *)&addr_in,
                            &addr_size) != -1)
            {
                uint8_t found = 0;
                for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
                {
                    if (addr_in.sin_port == htons(ports[i]))
                    {
                        found = 1;
                        clock_gettime(CLOCK_REALTIME, &elevator_times[i]);
                        system->elevators[i] = elevator;
                    }
                }
                if (!found || addr_in.sin_port == htons(ports[index]))
                {
                    LOG_ERROR("Wrong port from recv %d\n", errno);
                }
            }
        } while (0);

        register_orders(system->elevators, elevator_times, index);

        /* If floor/state change: update */
        if (system->elevators[index].current_floor != previous_state.current_floor)
        {
            driver_set_floor_indicator(system->elevator_socket, system->elevators[index].current_floor);
        }
        for (uint8_t i = 0; i < FLOOR_COUNT; ++i)
        {
            if (system->elevators[index].floor_states[i] != previous_state.floor_states[i])
            {
                driver_set_button_lamp(system->elevator_socket, system->elevators[index].floor_states[i], i);
            }
        }

        /* Monitor if elevator is stuck while moving. If so set the elevator to disabled */
        if (system->elevators[index].state == ELEVATOR_STATE_MOVING)
        {
            if (previous_state.current_floor != system->elevators[index].current_floor)
            {
                disable_timer = elevator_times[index];
                system->elevators[index].disabled = 0;
            }
            else if (disable_timer.tv_sec + DISABLED_TIMEOUT < elevator_times[index].tv_sec)
            {
                system->elevators[index].disabled = 1;
            }
        }

        /* Stop elevator at floor if it has an order there */
        if (system->elevators[index].state == ELEVATOR_STATE_MOVING && floor_signal_err >= 0)
        {
            /* We only stop if all elevators agree that we are taking this call */
            if (floor_is_locked(system->elevators, elevator_times, index))
            {
                driver_set_motor_direction(system->elevator_socket, MOTOR_DIRECTION_STOP);
                system->elevators[index].state = ELEVATOR_STATE_OPEN;
                driver_set_door_open_lamp(system->elevator_socket, 1);
                clock_gettime(CLOCK_REALTIME, &door_timer);
                disable_timer = door_timer;
                door_timer.tv_sec += DOOR_OPEN_TIME_SEC;
            }
        }

        /* Handle door timing */
        clock_gettime(CLOCK_REALTIME, &elevator_times[index]);
        if (system->elevators[index].state == ELEVATOR_STATE_OPEN)
        {
            struct timespec current_time;
            clock_gettime(CLOCK_REALTIME, &current_time);
            /* If stuck too long in open state, mark as disabled */
            if (disable_timer.tv_sec + DISABLED_TIMEOUT < current_time.tv_sec)
            {
                system->elevators[index].disabled = 1;
            }
            /* Extend door timer if obstructed */
            if (driver_get_obstruction_signal(system->elevator_socket))
            {
                door_timer = current_time;
                door_timer.tv_sec += DOOR_OPEN_TIME_SEC;
            }
            /* Complete order and continue */
            else if (current_time.tv_sec > door_timer.tv_sec)
            {
                complete_order(&system->elevators[index], system->elevator_socket, index);
                if (system->elevators[index].target_floor == system->elevators[index].current_floor)
                {
                    system->elevators[index].state = ELEVATOR_STATE_IDLE;
                }
                else
                {
                    system->elevators[index].state = ELEVATOR_STATE_MOVING;
                    disable_timer = elevator_times[index];
                    if (system->elevators[index].target_floor > system->elevators[index].current_floor)
                    {
                        driver_set_motor_direction(system->elevator_socket, MOTOR_DIRECTION_UP);
                    }
                    else
                    {
                        driver_set_motor_direction(system->elevator_socket, MOTOR_DIRECTION_DOWN);
                    }
                }
            }
        }

        /* Lock available orders in our direction of movement */
        if (system->elevators[index].state != ELEVATOR_STATE_IDLE)
        {
            /* Up direction order locking */
            if (system->elevators[index].direction == ELEVATOR_DIRECTION_UP)
            {
                for (size_t i = system->elevators[index].current_floor; i < FLOOR_COUNT; i++)
                {
                    if (order_is_available(system->elevators, elevator_times, &elevator_times[index],
                                           ELEVATOR_DIRECTION_UP, i))
                    {
                        system->elevators[index].floor_states[i] |= FLOOR_FLAG_LOCKED_UP;
                        system->elevators[index].locking_elevator[0][i] = index;
                        if (system->elevators[index].target_floor < i)
                        {
                            system->elevators[index].target_floor = i;
                        }
                    }
                    if ((system->elevators[index].floor_states[i] & FLOOR_FLAG_BUTTON_CAB) &&
                        system->elevators[index].target_floor < i)
                    {
                        system->elevators[index].target_floor = i;
                    }
                }
            }

            /* Down direction order locking */
            if (system->elevators[index].direction == ELEVATOR_DIRECTION_DOWN)
            {
                for (size_t i = system->elevators[index].current_floor; i > 0; i--)
                {
                    if (order_is_available(system->elevators, elevator_times, &elevator_times[index],
                                           ELEVATOR_DIRECTION_DOWN, i))
                    {
                        system->elevators[index].floor_states[i] |= FLOOR_FLAG_LOCKED_DOWN;
                        system->elevators[index].locking_elevator[1][i] = index;
                        if (system->elevators[index].target_floor > i)
                        {
                            system->elevators[index].target_floor = i;
                        }
                    }
                    if ((system->elevators[index].floor_states[i] & FLOOR_FLAG_BUTTON_CAB) &&
                        system->elevators[index].target_floor > i)
                    {
                        system->elevators[index].target_floor = i;
                    }
                }
            }
        }

        if (system->elevators[index].state != ELEVATOR_STATE_IDLE)
        {
            continue;
        }

        /* Check for cab calls */
        for (system->elevators[index].target_floor = 0; system->elevators[index].target_floor < FLOOR_COUNT;
             ++system->elevators[index].target_floor)
        {
            if ((system->elevators[index].floor_states[system->elevators[index].target_floor] &
                 FLOOR_FLAG_BUTTON_CAB) == 0)
            {
                continue;
            }

            if (system->elevators[index].target_floor > system->elevators[index].current_floor)
            {
                system->elevators[index].direction = ELEVATOR_DIRECTION_UP;
                system->elevators[index].locking_elevator[0][system->elevators[index].target_floor] = index;
                system->elevators[index].floor_states[system->elevators[index].target_floor] |= FLOOR_FLAG_LOCKED_UP;
                system->elevators[index].state = ELEVATOR_STATE_MOVING;
                disable_timer = elevator_times[index];
                driver_set_motor_direction(system->elevator_socket, MOTOR_DIRECTION_UP);
            }
            if (system->elevators[index].target_floor < system->elevators[index].current_floor)
            {
                system->elevators[index].direction = ELEVATOR_DIRECTION_DOWN;
                system->elevators[index].locking_elevator[1][system->elevators[index].target_floor] = index;
                system->elevators[index].floor_states[system->elevators[index].target_floor] |= FLOOR_FLAG_LOCKED_DOWN;
                system->elevators[index].state = ELEVATOR_STATE_MOVING;
                disable_timer = elevator_times[index];
                driver_set_motor_direction(system->elevator_socket, MOTOR_DIRECTION_DOWN);
            }
            if (system->elevators[index].target_floor == system->elevators[index].current_floor)
            {
                system->elevators[index].state = ELEVATOR_STATE_OPEN;
                driver_set_door_open_lamp(system->elevator_socket, 1);
                clock_gettime(CLOCK_REALTIME, &door_timer);
                door_timer.tv_sec += DOOR_OPEN_TIME_SEC;
            }
            break;
        }

        if (system->elevators[index].state != ELEVATOR_STATE_IDLE)
        {
            continue;
        }

        /* Check for hall calls */
        for (system->elevators[index].target_floor = 0; system->elevators[index].target_floor < FLOOR_COUNT;
             ++system->elevators[index].target_floor)
        {
            uint8_t do_call = FLOOR_FLAG_BUTTON_DOWN | FLOOR_FLAG_BUTTON_UP;
            /* Check if all elevators verify and agree a valid call */
            for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
            {
                /* Ignore disconnected elevators */
                if ((elevator_times[i].tv_sec + ELEVATOR_DISCONNECTED_TIME_SEC < elevator_times[index].tv_sec))
                {
                    continue;
                }
                do_call &= system->elevators[i].floor_states[system->elevators[index].target_floor];
            }
            /* If no shared order at this floor, continue */
            if (do_call == 0)
            {
                continue;
            }
            /* Hall UP */
            if (do_call & FLOOR_FLAG_BUTTON_UP)
            {
                if (!(system->elevators[index].floor_states[system->elevators[index].target_floor] &
                      FLOOR_FLAG_LOCKED_UP))
                {
                    system->elevators[index].floor_states[system->elevators[index].target_floor] |=
                        FLOOR_FLAG_LOCKED_UP;
                    system->elevators[index].locking_elevator[0][system->elevators[index].target_floor] = index;
                    break;
                }
                if (!verify_locked_floors(system->elevators, elevator_times, ELEVATOR_DIRECTION_UP, index))
                {
                    continue;
                }
                if (system->elevators[index].locking_elevator[0][system->elevators[index].target_floor] != index)
                {
                    continue;
                }

                /* Valid hall UP order, start moving */
                system->elevators[index].direction = ELEVATOR_DIRECTION_UP;
            }
            /* Hall DOWN */
            else
            {

                if (!(system->elevators[index].floor_states[system->elevators[index].target_floor] &
                      FLOOR_FLAG_LOCKED_DOWN))
                {
                    system->elevators[index].floor_states[system->elevators[index].target_floor] |=
                        FLOOR_FLAG_LOCKED_DOWN;
                    system->elevators[index].locking_elevator[1][system->elevators[index].target_floor] = index;
                    break;
                }
                if (!verify_locked_floors(system->elevators, elevator_times, ELEVATOR_DIRECTION_DOWN, index))
                {
                    continue;
                }
                if (system->elevators[index].locking_elevator[1][system->elevators[index].target_floor] != index)
                {
                    continue;
                }
                /* Valid hall DOWN order, start moving */
                system->elevators[index].direction = ELEVATOR_DIRECTION_DOWN;
            }

            /* Start moving UP or DOWN or opening doors depending on the relation between current_floor and target_floor
             */
            if (system->elevators[index].target_floor > system->elevators[index].current_floor)
            {
                system->elevators[index].state = ELEVATOR_STATE_MOVING;
                disable_timer = elevator_times[index];
                driver_set_motor_direction(system->elevator_socket, MOTOR_DIRECTION_UP);
            }
            if (system->elevators[index].target_floor < system->elevators[index].current_floor)
            {
                system->elevators[index].state = ELEVATOR_STATE_MOVING;
                disable_timer = elevator_times[index];
                driver_set_motor_direction(system->elevator_socket, MOTOR_DIRECTION_DOWN);
            }
            if (system->elevators[index].target_floor == system->elevators[index].current_floor)
            {
                system->elevators[index].state = ELEVATOR_STATE_OPEN;
                driver_set_door_open_lamp(system->elevator_socket, 1);
                clock_gettime(CLOCK_REALTIME, &door_timer);
                door_timer.tv_sec += DOOR_OPEN_TIME_SEC;
            }
            break;
        }
    }
}