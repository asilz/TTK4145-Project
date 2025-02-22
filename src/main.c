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

#ifndef ELEVATOR_COUNT
#define ELEVATOR_COUNT 1
#endif

#ifndef MASTER_PORT
#define MASTER_PORT 17532
#endif

#ifndef SLAVE_PORT
#define SLAVE_PORT 17533
#endif

enum FloorFlags
{
    FLOOR_FLAG_BUTTON_UP = 0x1,
    FLOOR_FLAG_BUTTON_DOWN = 0x2,
    FLOOR_FLAG_LOCKED = 0x4
};

enum ElevatorState
{
    ELEVATOR_STATE_IDLE = 0,
    ELEVATOR_STATE_MOVING = 1
};

static struct
{
    uint8_t floor_states[FLOOR_COUNT];
    pthread_mutex_t lock;
} context = {.floor_states = {0}};

void *thread_routine(void *args)
{
    socket_t *tcp_socket = ((socket_t *)(args));
    size_t current_floor = 0;
    size_t target_floor = 0;
    uint8_t cab_buttons[FLOOR_COUNT] = {0};
    int err = 0;
    enum ElevatorState current_state = ELEVATOR_STATE_IDLE;
    while (1)
    {

        struct Packet packet = {.command = COMMAND_TYPE_ORDER_BUTTON_ALL};
        pthread_mutex_lock(&context.lock);
        memcpy(packet.order_button_all_data.floor_states, context.floor_states, sizeof(context.floor_states));
        err = tcp_socket->vfptr->send_recv(tcp_socket, &packet);
        if (err != 0)
        {
            LOG_ERROR("send err = %d", err);
        }

        for (size_t i = 0; i < FLOOR_COUNT; ++i)
        {
            context.floor_states[i] |=
                (packet.order_button_all_data.floor_states[i] & 1) | (packet.order_button_all_data.floor_states[i] & 2);
            cab_buttons[i] = packet.order_button_all_data.floor_states[i] & 4;
        }
        pthread_mutex_unlock(&context.lock);

        packet.command = COMMAND_TYPE_FLOOR_SENSOR;
        err = tcp_socket->vfptr->send_recv(tcp_socket, &packet);
        if (err != 0)
        {
            LOG_ERROR("send err = %d", err);
        }

        /*

        LOG_INFO("Info loop: current_floor = %zu, target_floor = %zu, current_state = %d, calls: %u, %u, %u | %u, "
                 "%u, %u | %u, %u, %u | %u, %u, %u\n",
                 current_floor, target_floor, current_state, context.floor_states[0] & 1, context.floor_states[0] & 2,
                 context.floor_states[0] & 4, context.floor_states[1] & 1, context.floor_states[1] & 2,
                 context.floor_states[1] & 4, context.floor_states[2] & 1, context.floor_states[2] & 2,
                 context.floor_states[2] & 4, context.floor_states[3] & 1, context.floor_states[3] & 2,
                 context.floor_states[3] & 4);
                 */

        if (current_state == ELEVATOR_STATE_MOVING && current_floor == target_floor)
        {
            packet.command = COMMAND_TYPE_MOTOR_DIRECTION;
            packet.motor_direction_data.motor_direction = -1;
            tcp_socket->vfptr->send_recv(tcp_socket, &packet);
            current_state = ELEVATOR_STATE_IDLE;
            cab_buttons[target_floor] = 0;
            // TODO: Open doors
            pthread_mutex_lock(&context.lock);
            context.floor_states[target_floor] = 0;
            pthread_mutex_unlock(&context.lock);
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
                    err = tcp_socket->vfptr->send_recv(tcp_socket, &packet);
                    if (err != 0)
                    {
                        LOG_ERROR("recv err = %d", err);
                    }
                }
                if (target_floor < current_floor)
                {
                    packet.motor_direction_data.motor_direction = -1;
                    err = tcp_socket->vfptr->send_recv(tcp_socket, &packet);
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
                    err = tcp_socket->vfptr->send_recv(tcp_socket, &packet);
                    if (err != 0)
                    {
                        LOG_ERROR("recv err = %d", err);
                    }
                }
                if (target_floor < current_floor)
                {
                    packet.motor_direction_data.motor_direction = -1;
                    err = tcp_socket->vfptr->send_recv(tcp_socket, &packet);
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

    /*
    struct sockaddr_in addr_in;
    addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr_in.sin_port = htons(15659);
    addr_in.sin_family = AF_INET;

    elevator_init(&sockets[0], &addr_in);
    pthread_create(&pthread[0], NULL, thread_routine, &sockets[0]);
    */

    for (size_t i = 1; i < ELEVATOR_COUNT; ++i)
    {
        pthread_join(pthread[i], NULL);
    }

    pthread_mutex_destroy(&context.lock);

    return 0;
}
