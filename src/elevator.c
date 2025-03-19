#include <elevator.h>
#include <errno.h>
#include <log.h>
#include <netinet/ip.h>
#include <process.h>
#include <time.h>

#define ELEVATOR_DISCONNECTED_TIME_SEC (6)
#define DOOR_OPEN_TIME_SEC (3)

enum floor_flags_t
{
    FLOOR_FLAG_BUTTON_UP = 0x1,
    FLOOR_FLAG_BUTTON_DOWN = 0x2,
    FLOOR_FLAG_BUTTON_CAB = 0x4,
    FLOOR_FLAG_LOCKED = 0x8
};

void elevator_run(system_state_t *system, const uint16_t *ports, uint8_t index)
{
    struct timespec door_timer;
    struct timespec elevator_times[ELEVATOR_COUNT];
    for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
    {
        clock_gettime(CLOCK_REALTIME, &elevator_times[i]);
    }

    elevator_reload_config(system->elevator_socket);

    for (size_t i = 0; i < FLOOR_COUNT; ++i)
    {
        elevator_set_button_lamp(system->elevator_socket, system->elevators[index].floor_states[i], i);
    }
    elevator_set_floor_indicator(system->elevator_socket, system->elevators[index].current_floor);

    int err = elevator_get_floor_sensor_signal(system->elevator_socket);
    if (err < 0)
    {
        elevator_set_motor_direction(system->elevator_socket, MOTOR_DIRECTION_UP);
        while (elevator_get_floor_sensor_signal(system->elevator_socket) < 0)
        {
        }
        elevator_set_motor_direction(system->elevator_socket, MOTOR_DIRECTION_STOP);
    }

    system->elevators[index].state = ELEVATOR_STATE_IDLE;

    while (1)
    {
        uint8_t floor_states[FLOOR_COUNT] = {0};

        elevator_t previous_state = system->elevators[index];

        elevator_get_button_signals(system->elevator_socket, floor_states);
        for (size_t i = 0; i < FLOOR_COUNT; ++i)
        {
            system->elevators[index].floor_states[i] |= floor_states[i];
            LOG_INFO("floor_state %zu = %u\n", i, system->elevators[index].floor_states[i]);
        }
        int err = elevator_get_floor_sensor_signal(system->elevator_socket);
        if (err >= 0)
        {
            system->elevators[index].current_floor = err;
        }

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

        LOG_INFO("index = %" PRIu8 ", current_floor = %" PRIu8 ",target_floor = %" PRIu8 ", current_state = %" PRIu8
                 "\n",
                 index, system->elevators[index].current_floor, system->elevators[index].target_floor,
                 system->elevators[index].state);

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

        for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
        {
            if (i == index ||
                (elevator_times[i].tv_sec + ELEVATOR_DISCONNECTED_TIME_SEC < elevator_times[index].tv_sec))
            {
                continue;
            }
            for (size_t j = 0; j < FLOOR_COUNT; ++j)
            {
                if ((system->elevators[index].floor_states[j] & FLOOR_FLAG_BUTTON_DOWN) ||
                    (system->elevators[index].floor_states[j] & FLOOR_FLAG_BUTTON_UP))
                {
                    if (system->elevators[index].floor_states[j] & FLOOR_FLAG_LOCKED)
                    {
                        if (system->elevators[i].floor_states[j] == 0)
                        {
                            system->elevators[index].floor_states[j] &= FLOOR_FLAG_BUTTON_CAB;
                        }
                        if (system->elevators[i].floor_states[j] & FLOOR_FLAG_LOCKED)
                        {
                            if (system->elevators[i].locking_elevator[j] < system->elevators[index].locking_elevator[j])
                            {
                                system->elevators[index].locking_elevator[j] = system->elevators[i].locking_elevator[j];
                            }
                            else
                            {
                                system->elevators[i].locking_elevator[j] = system->elevators[index].locking_elevator[j];
                            }
                        }
                    }
                    else
                    {
                        if (system->elevators[i].floor_states[j] & FLOOR_FLAG_LOCKED)
                        {
                            system->elevators[index].floor_states[j] |=
                                system->elevators[i].floor_states[j] &
                                (FLOOR_FLAG_BUTTON_DOWN | FLOOR_FLAG_BUTTON_UP | FLOOR_FLAG_LOCKED);
                            system->elevators[index].locking_elevator[j] = system->elevators[i].locking_elevator[j];
                        }
                    }
                }
                else
                {
                    if (((system->elevators[i].floor_states[j] & FLOOR_FLAG_BUTTON_DOWN) ||
                         (system->elevators[i].floor_states[j] & FLOOR_FLAG_BUTTON_UP)) &&
                        ((system->elevators[i].floor_states[j] & FLOOR_FLAG_LOCKED) == 0))
                    {
                        system->elevators[index].floor_states[j] |=
                            system->elevators[i].floor_states[j] &
                            (FLOOR_FLAG_BUTTON_DOWN | FLOOR_FLAG_BUTTON_UP | FLOOR_FLAG_LOCKED);
                    }
                }
            }
        }

        if (system->elevators[index].current_floor != previous_state.current_floor)
        {
            elevator_set_floor_indicator(system->elevator_socket, system->elevators[index].current_floor);
        }
        for (uint8_t i = 0; i < FLOOR_COUNT; ++i)
        {
            if (system->elevators[index].floor_states[i] != previous_state.floor_states[i])
            {
                elevator_set_button_lamp(system->elevator_socket, system->elevators[index].floor_states[i], i);
            }
        }

        if (system->elevators[index].state == ELEVATOR_STATE_MOVING &&
            system->elevators[index].current_floor == system->elevators[index].target_floor)
        {
            elevator_set_motor_direction(system->elevator_socket, MOTOR_DIRECTION_STOP);
            system->elevators[index].state = ELEVATOR_STATE_OPEN;
            elevator_set_door_open_lamp(system->elevator_socket, 1);
            clock_gettime(CLOCK_REALTIME, &door_timer);
            door_timer.tv_sec += DOOR_OPEN_TIME_SEC;
        }

        clock_gettime(CLOCK_REALTIME, &elevator_times[index]);
        if (system->elevators[index].state == ELEVATOR_STATE_OPEN)
        {
            struct timespec current_time;
            clock_gettime(CLOCK_REALTIME, &current_time);

            if (elevator_get_obstruction_signal(system->elevator_socket))
            {
                door_timer = current_time;
                door_timer.tv_sec += DOOR_OPEN_TIME_SEC;
            }
            else if (current_time.tv_sec > door_timer.tv_sec)
            {
                elevator_set_door_open_lamp(system->elevator_socket, 0);
                system->elevators[index].floor_states[system->elevators[index].target_floor] = 0;
                system->elevators[index].locking_elevator[system->elevators[index].target_floor] = 255;
                system->elevators[index].state = ELEVATOR_STATE_IDLE;
                elevator_set_button_lamp(system->elevator_socket,
                                         system->elevators[index].floor_states[system->elevators[index].target_floor],
                                         system->elevators[index].target_floor);
            }
        }

        if (system->elevators[index].state != ELEVATOR_STATE_IDLE)
        {
            continue;
        }

        for (system->elevators[index].target_floor = 0; system->elevators[index].target_floor < FLOOR_COUNT;
             ++system->elevators[index].target_floor)
        {
            if ((system->elevators[index].floor_states[system->elevators[index].target_floor] &
                 FLOOR_FLAG_BUTTON_CAB) == 0)
            {
                continue;
            }

            system->elevators[index].floor_states[system->elevators[index].target_floor] |= FLOOR_FLAG_LOCKED;
            system->elevators[index].locking_elevator[system->elevators[index].target_floor] = index;

            if (system->elevators[index].target_floor > system->elevators[index].current_floor)
            {
                system->elevators[index].state = ELEVATOR_STATE_MOVING;
                elevator_set_motor_direction(system->elevator_socket, MOTOR_DIRECTION_UP);
            }
            if (system->elevators[index].target_floor < system->elevators[index].current_floor)
            {
                system->elevators[index].state = ELEVATOR_STATE_MOVING;
                elevator_set_motor_direction(system->elevator_socket, MOTOR_DIRECTION_DOWN);
            }
            if (system->elevators[index].target_floor == system->elevators[index].current_floor)
            {
                system->elevators[index].state = ELEVATOR_STATE_OPEN;
                elevator_set_door_open_lamp(system->elevator_socket, 1);
                clock_gettime(CLOCK_REALTIME, &door_timer);
                door_timer.tv_sec += DOOR_OPEN_TIME_SEC;
            }
            break;
        }
        if (system->elevators[index].state != ELEVATOR_STATE_IDLE)
        {
            continue;
        }
        for (system->elevators[index].target_floor = 0; system->elevators[index].target_floor < FLOOR_COUNT;
             ++system->elevators[index].target_floor)
        {
            if (!((system->elevators[index].floor_states[system->elevators[index].target_floor] & FLOOR_FLAG_LOCKED) &&
                  system->elevators[index].locking_elevator[index] == index))
            {
                uint8_t do_call = FLOOR_FLAG_BUTTON_DOWN | FLOOR_FLAG_BUTTON_UP;
                for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
                {
                    if ((elevator_times[i].tv_sec + ELEVATOR_DISCONNECTED_TIME_SEC < elevator_times[index].tv_sec))
                    {
                        continue;
                    }
                    if (system->elevators[i].floor_states[system->elevators[index].target_floor] &
                        FLOOR_FLAG_BUTTON_CAB)
                    {
                        do_call = 0;
                        break;
                    }
                    do_call &= system->elevators[i].floor_states[system->elevators[index].target_floor];
                }
                if (do_call == 0)
                {
                    continue;
                }
                if (!(system->elevators[index].floor_states[system->elevators[index].target_floor] & FLOOR_FLAG_LOCKED))
                {
                    system->elevators[index].floor_states[system->elevators[index].target_floor] |= FLOOR_FLAG_LOCKED;
                    system->elevators[index].locking_elevator[system->elevators[index].target_floor] = index;
                    break;
                }
                if (system->elevators[index].locking_elevator[system->elevators[index].target_floor] != index)
                {
                    break;
                }
                for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
                {
                    if ((elevator_times[i].tv_sec + ELEVATOR_DISCONNECTED_TIME_SEC < elevator_times[index].tv_sec))
                    {
                        continue;
                    }
                    if (system->elevators[index].locking_elevator[system->elevators[index].target_floor] !=
                        system->elevators[i].locking_elevator[system->elevators[index].target_floor])
                    {
                        do_call = 0;
                        break;
                    }
                }
                if (do_call == 0)
                {
                    continue;
                }
            }

            if (system->elevators[index].target_floor > system->elevators[index].current_floor)
            {
                system->elevators[index].state = ELEVATOR_STATE_MOVING;
                elevator_set_motor_direction(system->elevator_socket, MOTOR_DIRECTION_UP);
            }
            if (system->elevators[index].target_floor < system->elevators[index].current_floor)
            {
                system->elevators[index].state = ELEVATOR_STATE_MOVING;
                elevator_set_motor_direction(system->elevator_socket, MOTOR_DIRECTION_DOWN);
            }
            if (system->elevators[index].target_floor == system->elevators[index].current_floor)
            {
                system->elevators[index].state = ELEVATOR_STATE_OPEN;
                elevator_set_door_open_lamp(system->elevator_socket, 1);
                clock_gettime(CLOCK_REALTIME, &door_timer);
                door_timer.tv_sec += DOOR_OPEN_TIME_SEC;
            }
            break;
        }
    }
}