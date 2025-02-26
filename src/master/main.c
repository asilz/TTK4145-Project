#include <driver.h>
#include <log.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifndef MASTER_PORT
#define MASTER_PORT 17532
#endif

#ifndef SLAVE_PORT
#define SLAVE_PORT 17533
#endif

enum floor_flags_t
{
    FLOOR_FLAG_BUTTON_UP = 0x1,
    FLOOR_FLAG_BUTTON_DOWN = 0x2,
    FLOOR_FLAG_LOCKED = 0x4
};

enum elevator_state_t
{
    ELEVATOR_STATE_IDLE = 0,
    ELEVATOR_STATE_MOVING = 1,
    ELEVATOR_STATE_OPEN = 2
};

static struct
{
    uint8_t floor_states[FLOOR_COUNT];
    pthread_mutex_t lock;

    pthread_cond_t cond;
    pthread_cond_t active_threads_cond;
    pthread_mutex_t elevator_lock;
    size_t active_threads;
    bool is_slave;
    socket_t elevator_sock;
} context = {.floor_states = {0}};

void *thread_routine(void *args)
{
    socket_t *socket = ((socket_t *)(args));
    size_t current_floor = 0;
    size_t target_floor = 0;
    uint8_t cab_buttons[FLOOR_COUNT] = {0};

    struct packet_t packet_reload_config = {.command = COMMAND_TYPE_RELOAD_CONFIG};
    int err = socket_send_recv(socket, &packet_reload_config);
    if (err != 0)
    {
        LOG_ERROR("send err = %d", err);
    }
    enum elevator_state_t current_state = ELEVATOR_STATE_IDLE;
    struct timespec door_timer;

    while (1)
    {
        if (current_state == ELEVATOR_STATE_IDLE)
        {
            pthread_mutex_lock(&context.elevator_lock);
            while (context.is_slave)
            {
                pthread_cond_wait(&context.cond, &context.elevator_lock);
            }
            context.active_threads++;
            pthread_mutex_unlock(&context.elevator_lock);
        }

        struct packet_t packet = {.command = COMMAND_TYPE_ORDER_BUTTON_ALL};
        pthread_mutex_lock(&context.lock);
        memcpy(packet.order_button_all_data.floor_states, context.floor_states, sizeof(context.floor_states));
        err = socket_send_recv(socket, &packet);
        if (err != 0)
        {
            LOG_ERROR("send err = %d", err);
        }

        for (size_t i = 0; i < FLOOR_COUNT; ++i)
        {
            context.floor_states[i] |=
                (packet.order_button_all_data.floor_states[i] & 1) | (packet.order_button_all_data.floor_states[i] & 2);
            cab_buttons[i] |= packet.order_button_all_data.floor_states[i] & 4;
            packet.order_button_light_all_data.floor_lights[i] = context.floor_states[i] | cab_buttons[i];
        }
        pthread_mutex_unlock(&context.lock);

        packet.command = COMMAND_TYPE_FLOOR_SENSOR;
        err = socket_send_recv(socket, &packet);
        if (err != 0)
        {
            LOG_ERROR("send err = %d", err);
        }
        if (packet.floor_sensor_data.output.at_floor)
        {
            current_floor = packet.floor_sensor_data.output.floor;
        }

        LOG_INFO("Info loop: current_floor = %zu, target_floor = %zu, current_state = %d, calls: %u, %u, %u | %u, "
                 "%u, %u | %u, %u, %u | %u, %u, %u\n",
                 current_floor, target_floor, current_state, context.floor_states[0] & 1, context.floor_states[0] & 2,
                 context.floor_states[0] & 4, context.floor_states[1] & 1, context.floor_states[1] & 2,
                 context.floor_states[1] & 4, context.floor_states[2] & 1, context.floor_states[2] & 2,
                 context.floor_states[2] & 4, context.floor_states[3] & 1, context.floor_states[3] & 2,
                 context.floor_states[3] & 4);

        if (current_state == ELEVATOR_STATE_MOVING && current_floor == target_floor)
        {
            packet.command = COMMAND_TYPE_MOTOR_DIRECTION;
            packet.motor_direction_data.motor_direction = 0;
            socket_send_recv(socket, &packet);
            current_state = ELEVATOR_STATE_OPEN;

            packet.command = COMMAND_TYPE_DOOR_OPEN_LIGHT;
            packet.door_open_light_data.value = 1;
            socket_send_recv(socket, &packet);

            clock_gettime(CLOCK_REALTIME, &door_timer);
            door_timer.tv_sec += 3;
        }

        if (current_state == ELEVATOR_STATE_OPEN)
        {
            struct timespec current_time;
            clock_gettime(CLOCK_REALTIME, &current_time);

            packet.command = COMMAND_TYPE_OBSTRUCTION_SWITCH;
            socket_send_recv(socket, &packet);
            if (packet.obstruction_switch_data.output.active)
            {
                door_timer = current_time;
                door_timer.tv_sec += 3;
            }
            else if (current_time.tv_sec > door_timer.tv_sec)
            {
                packet.command = COMMAND_TYPE_DOOR_OPEN_LIGHT;
                packet.door_open_light_data.value = 0;
                socket_send_recv(socket, &packet);
                cab_buttons[target_floor] = 0;
                pthread_mutex_lock(&context.lock);
                context.floor_states[target_floor] = 0;
                pthread_mutex_unlock(&context.lock);
                current_state = ELEVATOR_STATE_IDLE;

                pthread_mutex_lock(&context.elevator_lock);
                if (--context.active_threads == 0)
                {
                    pthread_cond_signal(&context.active_threads_cond);
                }
                pthread_mutex_unlock(&context.elevator_lock);
            }
        }

        if (current_state != ELEVATOR_STATE_IDLE)
        {
            continue;
        }

        for (target_floor = 0; target_floor < FLOOR_COUNT; ++target_floor)
        {
            if (cab_buttons[target_floor])
            {
                pthread_mutex_lock(&context.lock);
                context.floor_states[target_floor] |= FLOOR_FLAG_LOCKED;
                pthread_mutex_unlock(&context.lock);
                current_state = ELEVATOR_STATE_MOVING;
                packet.command = COMMAND_TYPE_MOTOR_DIRECTION;
                if (target_floor > current_floor)
                {
                    packet.motor_direction_data.motor_direction = 1;
                    memset((uint8_t *)(&packet) + 2, 0,
                           sizeof(packet) - 2); // Sets the two last bytes in the packet to 0
                    err = socket_send_recv(socket, &packet);
                    if (err != 0)
                    {
                        LOG_ERROR("recv err = %d", err);
                    }
                }
                if (target_floor < current_floor)
                {
                    packet.motor_direction_data.motor_direction = -1;
                    memset((uint8_t *)(&packet) + 2, 0,
                           sizeof(packet) - 2); // Sets the two last bytes in the packet to 0
                    err = socket_send_recv(socket, &packet);
                    if (err != 0)
                    {
                        LOG_ERROR("recv err = %d", err);
                    }
                }
                break;
            }
        }

        if (current_state != ELEVATOR_STATE_IDLE)
        {
            continue;
        }

        pthread_mutex_lock(&context.lock);
        for (target_floor = 0; target_floor < FLOOR_COUNT; ++target_floor)
        {
            if ((context.floor_states[target_floor] & (FLOOR_FLAG_BUTTON_DOWN | FLOOR_FLAG_BUTTON_UP)) &&
                (!(context.floor_states[target_floor] & FLOOR_FLAG_LOCKED)))
            {
                if (current_floor == target_floor)
                {
                    context.floor_states[target_floor] = 0; // TODO: Open doors
                    continue;
                }
                packet.command = COMMAND_TYPE_MOTOR_DIRECTION;
                context.floor_states[target_floor] |= FLOOR_FLAG_LOCKED;
                if (target_floor > current_floor)
                {
                    packet.motor_direction_data.motor_direction = 1;
                    memset((uint8_t *)(&packet) + 2, 0,
                           sizeof(packet) - 2); // Sets the two last bytes in the packet to 0
                    err = socket_send_recv(socket, &packet);
                    if (err != 0)
                    {
                        LOG_ERROR("recv err = %d", err);
                    }
                }
                if (target_floor < current_floor)
                {
                    packet.motor_direction_data.motor_direction = -1;
                    memset((uint8_t *)(&packet) + 2, 0,
                           sizeof(packet) - 2); // Sets the two last bytes in the packet to 0
                    err = socket_send_recv(socket, &packet);
                    if (err != 0)
                    {
                        LOG_ERROR("recv err = %d", err);
                    }
                }
                current_state = ELEVATOR_STATE_MOVING;
                break;
            }
        }
        pthread_mutex_unlock(&context.lock);
    }
}

void *slave_routine(void *args)
{
    socket_t *socket = ((socket_t *)(args));

    while (1)
    {
        struct packet_t packet;

        while (socket_recv(socket, &packet) != 0)
        {
        }
        pthread_mutex_lock(&context.elevator_lock);
        while (context.active_threads > 0)
        {
            pthread_cond_wait(&context.active_threads_cond, &context.elevator_lock);
        }
        do
        {
            socket_send_recv(&context.elevator_sock, &packet);
            socket_send(socket, &packet);
        } while (socket_recv(socket, &packet) == 0);
        context.is_slave = false;
        pthread_cond_broadcast(&context.cond);

        pthread_mutex_unlock(&context.elevator_lock);
    }
}

int main()
{
    pthread_mutex_init(&context.lock, NULL);

    socket_t sockets[ELEVATOR_COUNT];
    pthread_t pthread[ELEVATOR_COUNT];
    for (size_t i = 1; i < ELEVATOR_COUNT; ++i)
    {
        struct sockaddr_in addr_in;
        addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr_in.sin_port = htons(SLAVE_PORT);
        addr_in.sin_family = AF_INET;

        struct sockaddr_in bind_address = {
            .sin_addr.s_addr = htonl(INADDR_LOOPBACK), .sin_port = htons(MASTER_PORT), .sin_family = AF_INET};

        node_udp_init(&sockets[i], &addr_in, &bind_address);

        pthread_create(&pthread[i], NULL, thread_routine, &sockets[i]);
    }

    struct sockaddr_in addr_in;
    addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr_in.sin_port = htons(15657);
    addr_in.sin_family = AF_INET;

    elevator_init(&sockets[0], &addr_in);
    pthread_create(&pthread[0], NULL, thread_routine, &sockets[0]);

    for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
    {
        pthread_join(pthread[i], NULL);
    }

    pthread_mutex_destroy(&context.lock);

    return 0;
}
