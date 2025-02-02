#include <log.h>
#include <netinet/ip.h>
#include <network.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifndef FLOOR_COUNT
#define FLOOR_COUNT 4
#endif

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

static const struct Message msg_motor_up = {.command = COMMAND_TYPE_MOTOR_DIRECTION, .args = {1}};
static const struct Message msg_motor_down = {.command = COMMAND_TYPE_MOTOR_DIRECTION, .args = {-1}};
static const struct Message msg_motor_stop = {.command = COMMAND_TYPE_MOTOR_DIRECTION, .args = {0}};

void *thread_routine(void *args)
{
    Socket *tcp_socket = ((Socket *)(args));
    size_t current_floor = 0;
    size_t target_floor = 0;
    bool cab_buttons[FLOOR_COUNT] = {0};
    int err = 0;
    enum ElevatorState current_state = ELEVATOR_STATE_IDLE;
    while (1)
    {
        for (size_t i = 0; i < FLOOR_COUNT; ++i)
        {
            uint8_t floor_state = 0;

            struct Message msg = {.command = COMMAND_TYPE_ORDER_BUTTON, .args = {BUTTON_TYPE_HALL_UP, i}};
            err = tcp_socket->send(tcp_socket, &msg);
            if (err != 0)
            {
                LOG_ERROR("send err = %d", err);
            }
            err = tcp_socket->recv(tcp_socket, &msg);
            if (err != 0)
            {
                LOG_ERROR("recv err = %d", err);
            }
            floor_state = floor_state | msg.args[0];
            msg.args[0] = BUTTON_TYPE_HALL_DOWN;
            msg.args[1] = i;
            err = tcp_socket->send(tcp_socket, &msg);
            if (err != 0)
            {
                LOG_ERROR("send err = %d", err);
            }
            err = tcp_socket->recv(tcp_socket, &msg);
            if (err != 0)
            {
                LOG_ERROR("recv err = %d", err);
            }
            floor_state = floor_state | (msg.args[0] << 1);

            if (floor_state != 0)
            {
                LOG_INFO("HALL BUTTON MASTER\n");
            }

            pthread_mutex_lock(&context.lock);
            context.floor_states[i] |= floor_state;
            pthread_mutex_unlock(&context.lock);

            msg.command = COMMAND_TYPE_ORDER_BUTTON;
            msg.args[0] = BUTTON_TYPE_CAB;
            msg.args[1] = i;
            err = tcp_socket->send(tcp_socket, &msg);
            if (err != 0)
            {
                LOG_ERROR("send err = %d", err);
            }
            err = tcp_socket->recv(tcp_socket, &msg);
            if (err != 0)
            {
                LOG_ERROR("recv err = %d", err);
            }
            if (msg.args[0])
            {
                cab_buttons[i] = true;
            }
            LOG_INFO("Info loop\n");
        }

        struct Message msg = {.command = COMMAND_TYPE_FLOOR_SENSOR};
        tcp_socket->send(tcp_socket, &msg);
        tcp_socket->recv(tcp_socket, &msg);
        current_floor = msg.args[1];

        if (current_state == ELEVATOR_STATE_MOVING && current_floor == target_floor && msg.args[0])
        {
            tcp_socket->send(tcp_socket, &msg_motor_stop);
            current_state = ELEVATOR_STATE_IDLE;
            cab_buttons[target_floor] = false;
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
                if (target_floor > current_floor)
                {
                    err = tcp_socket->send(tcp_socket, &msg_motor_up);
                    if (err != 0)
                    {
                        LOG_ERROR("recv err = %d", err);
                    }
                }
                if (target_floor < current_floor)
                {
                    err = tcp_socket->send(tcp_socket, &msg_motor_down);
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
                context.floor_states[target_floor] |= FLOOR_FLAG_LOCKED;
                if (target_floor > current_floor)
                {
                    err = tcp_socket->send(tcp_socket, &msg_motor_up);
                    if (err != 0)
                    {
                        LOG_ERROR("recv err = %d", err);
                    }
                }
                if (target_floor < current_floor)
                {
                    err = tcp_socket->send(tcp_socket, &msg_motor_down);
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

    Socket sockets[ELEVATOR_COUNT];
    pthread_t pthread[ELEVATOR_COUNT];
    for (size_t i = 1; i < ELEVATOR_COUNT; ++i)
    {

        struct sockaddr_in addr_in;
        addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr_in.sin_port = htons(SLAVE_PORT);
        addr_in.sin_family = AF_INET;

        struct sockaddr_in bind_address = {
            .sin_addr.s_addr = htonl(INADDR_LOOPBACK), .sin_port = htons(MASTER_PORT), .sin_family = AF_INET};

        socket_udp_init(&sockets[i], &addr_in, &bind_address);

        pthread_create(&pthread[i], NULL, thread_routine, &sockets[i]);
    }

    /*
        struct sockaddr_in addr_in;
        addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr_in.sin_port = htons(15659);
        addr_in.sin_family = AF_INET;

        socket_tcp_client_init(&sockets[0], &addr_in);
    */
    for (size_t i = 1; i < ELEVATOR_COUNT; ++i)
    {
        pthread_join(pthread[i], NULL);
    }

    pthread_mutex_destroy(&context.lock);

    return 0;
}
