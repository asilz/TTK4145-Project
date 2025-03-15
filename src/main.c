#include <assert.h>
#include <driver2.h>
#include <errno.h>
#include <inttypes.h>
#include <log.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef FLOOR_COUNT
#define FLOOR_COUNT 4
#endif

#ifndef ELEVATOR_COUNT
#define ELEVATOR_COUNT 3
#endif

enum floor_flags_t
{
    FLOOR_FLAG_BUTTON_UP = 0x1,
    FLOOR_FLAG_BUTTON_DOWN = 0x2,
    FLOOR_FLAG_BUTTON_CAB = 0x4,
    FLOOR_FLAG_LOCKED = 0x8
};

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

int main(int argc, char **argv)
{
    uint16_t ports[ELEVATOR_COUNT];

    struct sockaddr_in addr_in = {
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK), .sin_port = htons(15657), .sin_family = AF_INET};

    size_t index = 0;

    uint8_t end_parse = 1;
    size_t port_index = 0;
    while (end_parse)
    {
        switch (getopt(argc, argv, "p:e:i:"))
        {
        case 'i':
            sscanf(optarg, "%zu", &index);
            break;
        case 'p':
            sscanf(optarg, "%hu", &ports[port_index++]);
            break;
        case 'e':
            sscanf(optarg, "%hd", &addr_in.sin_port);
            addr_in.sin_port = htons(addr_in.sin_port);
            break;
        case -1:
            end_parse = 0;
            break; // Done parsing options
        }
    }

    socket_t elevator_socket;
    elevator_init(&elevator_socket, &addr_in);

    elevator_t elevators[ELEVATOR_COUNT] = {0};
    elevators[index].state = ELEVATOR_STATE_IDLE;

    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == -1)
    {
        LOG_ERROR("socket init error = %d\n", errno);
    }

    int value = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) == -1)
    {
        LOG_ERROR("Set reusable error = %d\n", errno);
        (void)close(fd);
    }
    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value)) == -1)
    {
        LOG_ERROR("Set broadcast error = %d\n", errno);
        (void)close(fd);
    }

    struct timeval time = {.tv_usec = 1, .tv_sec = 0};
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) == -1)
    {
        LOG_ERROR("Set timeout error = %d\n", errno);
        (void)close(fd);
    }

    addr_in = (struct sockaddr_in){
        .sin_addr.s_addr = htonl(INADDR_ANY), .sin_port = htons(ports[index]), .sin_family = AF_INET};

    if (bind(fd, (struct sockaddr *)&addr_in, sizeof(addr_in)) == -1)
    {
        LOG_ERROR("Bind failed error = %d\n", errno);
        (void)close(fd);
    }

    struct timespec door_timer;
    struct timespec elevator_times[ELEVATOR_COUNT];
    while (1)
    {
        uint8_t floor_states[FLOOR_COUNT] = {0};

        elevator_get_button_signals_(&elevator_socket, floor_states);
        for (size_t i = 0; i < FLOOR_COUNT; ++i)
        {
            elevators[index].floor_states[i] |= floor_states[i];
            LOG_INFO("floor_state %zu = %u\n", i, elevators[index].floor_states[i]);
        }
        int err = elevator_get_floor_sensor_signal_(&elevator_socket);
        if (err >= 0)
        {
            elevators[index].current_floor = err;
        }

        for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
        {
            if (i == index)
            {
                continue;
            }
            struct sockaddr_in broadcast_addr = {
                .sin_family = AF_INET, .sin_port = htons(ports[i]), .sin_addr.s_addr = INADDR_BROADCAST};
            int err = sendto(fd, &elevators[index], sizeof(*elevators), MSG_NOSIGNAL,
                             (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
            if (err == -1)
            {
                LOG_ERROR("broadcast error = %d\n", errno);
            }
        }

        LOG_INFO("index = %zu, current_floor = %u,target_floor = %u, current_state = %u\n", index,
                 elevators[index].current_floor, elevators[index].target_floor, elevators[index].state);

        do
        {
            elevator_t elevator;
            socklen_t addr_size = sizeof(addr_in);
            while (recvfrom(fd, &elevator, sizeof(elevator), MSG_NOSIGNAL, (struct sockaddr *)&addr_in, &addr_size) !=
                   -1)
            {
                uint8_t found = 0;
                for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
                {
                    if (addr_in.sin_port == htons(ports[i]))
                    {
                        found = 1;
                        clock_gettime(CLOCK_REALTIME, &elevator_times[i]);
                        elevators[i] = elevator;
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
            if (i == index || (elevator_times[i].tv_sec + 6 < elevator_times[index].tv_sec))
            {
                continue;
            }
            for (size_t j = 0; j < FLOOR_COUNT; ++j)
            {
                if ((elevators[index].floor_states[j] & FLOOR_FLAG_BUTTON_DOWN) ||
                    (elevators[index].floor_states[j] & FLOOR_FLAG_BUTTON_UP))
                {
                    if (elevators[index].floor_states[j] & FLOOR_FLAG_LOCKED)
                    {
                        if (elevators[i].floor_states[j] == 0)
                        {
                            elevators[index].floor_states[j] = 0;
                        }
                        if (elevators[i].floor_states[j] & FLOOR_FLAG_LOCKED)
                        {
                            if (elevators[i].locking_elevator[j] < elevators[index].locking_elevator[j])
                            {
                                elevators[index].locking_elevator[j] = elevators[i].locking_elevator[j];
                            }
                            else
                            {
                                elevators[i].locking_elevator[j] = elevators[index].locking_elevator[j];
                            }
                        }
                    }
                    else
                    {
                        if (elevators[i].floor_states[j] & FLOOR_FLAG_LOCKED)
                        {
                            elevators[index].floor_states[j] = elevators[i].floor_states[j];
                            elevators[index].locking_elevator[j] = elevators[i].locking_elevator[j];
                        }
                    }
                }
                else
                {
                    if (((elevators[i].floor_states[j] & FLOOR_FLAG_BUTTON_DOWN) ||
                         (elevators[i].floor_states[j] & FLOOR_FLAG_BUTTON_UP)) &&
                        ((elevators[i].floor_states[j] & FLOOR_FLAG_LOCKED) == 0))
                    {
                        elevators[index].floor_states[j] = elevators[i].floor_states[j];
                    }
                }
            }
        }

        if (elevators[index].state == ELEVATOR_STATE_MOVING &&
            elevators[index].current_floor == elevators[index].target_floor)
        {
            elevator_set_motor_direction_(&elevator_socket, 0);
            elevators[index].state = ELEVATOR_STATE_OPEN;
            elevator_set_door_open_lamp_(&elevator_socket, 1);
            clock_gettime(CLOCK_REALTIME, &door_timer);
            door_timer.tv_sec += 3;
        }

        clock_gettime(CLOCK_REALTIME, &elevator_times[index]);
        if (elevators[index].state == ELEVATOR_STATE_OPEN)
        {
            struct timespec current_time;
            clock_gettime(CLOCK_REALTIME, &current_time);

            if (elevator_get_obstruction_signal_(&elevator_socket))
            {
                door_timer = current_time;
                door_timer.tv_sec += 3;
            }
            else if (current_time.tv_sec > door_timer.tv_sec)
            {
                elevator_set_door_open_lamp_(&elevator_socket, 0);
                elevators[index].floor_states[elevators[index].target_floor] = 0;
                elevators[index].locking_elevator[elevators[index].target_floor] = 255;
                elevators[index].state = ELEVATOR_STATE_IDLE;
            }
        }

        if (elevators[index].state != ELEVATOR_STATE_IDLE)
        {
            continue;
        }

        for (elevators[index].target_floor = 0; elevators[index].target_floor < FLOOR_COUNT;
             ++elevators[index].target_floor)
        {
            if ((elevators[index].floor_states[elevators[index].target_floor] & FLOOR_FLAG_BUTTON_CAB) == 0)
            {
                continue;
            }

            if (elevators[index].target_floor > elevators[index].current_floor)
            {
                elevators[index].state = ELEVATOR_STATE_MOVING;
                elevator_set_motor_direction_(&elevator_socket, 1);
            }
            if (elevators[index].target_floor < elevators[index].current_floor)
            {
                elevators[index].state = ELEVATOR_STATE_MOVING;
                elevator_set_motor_direction_(&elevator_socket, -1);
            }
            if (elevators[index].target_floor == elevators[index].current_floor)
            {
                elevators[index].state = ELEVATOR_STATE_OPEN;
                elevator_set_door_open_lamp_(&elevator_socket, 1);
                clock_gettime(CLOCK_REALTIME, &door_timer);
                door_timer.tv_sec += 3;
            }
            break;
        }
        if (elevators[index].state != ELEVATOR_STATE_IDLE)
        {
            continue;
        }
        for (elevators[index].target_floor = 0; elevators[index].target_floor < FLOOR_COUNT;
             ++elevators[index].target_floor)
        {
            uint8_t do_call = FLOOR_FLAG_BUTTON_DOWN | FLOOR_FLAG_BUTTON_UP;
            for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
            {
                if ((elevator_times[i].tv_sec + 6 < elevator_times[index].tv_sec))
                {
                    continue;
                }
                if (elevators[i].floor_states[elevators[index].target_floor] & FLOOR_FLAG_BUTTON_CAB)
                {
                    do_call = 0;
                    break;
                }
                do_call &= elevators[i].floor_states[elevators[index].target_floor];
            }
            if (do_call == 0)
            {
                continue;
            }
            if (!(elevators[index].floor_states[elevators[index].target_floor] & FLOOR_FLAG_LOCKED))
            {
                elevators[index].floor_states[elevators[index].target_floor] |= FLOOR_FLAG_LOCKED;
                elevators[index].locking_elevator[elevators[index].target_floor] = index;
                break;
            }
            if (elevators[index].locking_elevator[elevators[index].target_floor] != index)
            {
                break;
            }
            for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
            {
                if ((elevator_times[i].tv_sec + 6 < elevator_times[index].tv_sec))
                {
                    continue;
                }
                if (elevators[index].locking_elevator[elevators[index].target_floor] !=
                    elevators[i].locking_elevator[elevators[index].target_floor])
                {
                    do_call = 0;
                    break;
                }
            }
            if (do_call == 0)
            {
                continue;
            }

            if (elevators[index].target_floor > elevators[index].current_floor)
            {
                elevators[index].state = ELEVATOR_STATE_MOVING;
                elevator_set_motor_direction_(&elevator_socket, 1);
            }
            if (elevators[index].target_floor < elevators[index].current_floor)
            {
                elevators[index].state = ELEVATOR_STATE_MOVING;
                elevator_set_motor_direction_(&elevator_socket, -1);
            }
            if (elevators[index].target_floor == elevators[index].current_floor)
            {
                elevators[index].state = ELEVATOR_STATE_OPEN;
                elevator_set_door_open_lamp_(&elevator_socket, 1);
                clock_gettime(CLOCK_REALTIME, &door_timer);
                door_timer.tv_sec += 3;
            }
            break;
        }
    }
}