#include <log.h>
#include <netinet/ip.h>
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

typedef enum CommandType
{
    RELOAD_CONFIG = 0,
    MOTOR_DIRECTION,
    ORDER_BUTTON_LIGHT,
    FLOOR_INDICATOR,
    DOOR_OPEN_LIGHT,
    STOP_BUTTON_LIGHT,
    ORDER_BUTTON,
    FLOOR_SENSOR,
    STOP_BUTTON,
    OBSTRUCTION_SWITCH,
} CommandType;

typedef enum ButtonType
{
    HALL_UP,
    HALL_DOWN,
    CAB,
} ButtonType;

struct Message
{
    int8_t command;
    int8_t args[3];
};

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

static const struct Message msg_motor_up = {.command = MOTOR_DIRECTION, .args = {1}};
static const struct Message msg_motor_down = {.command = MOTOR_DIRECTION, .args = {-1}};
static const struct Message msg_motor_stop = {.command = MOTOR_DIRECTION, .args = {0}};

void *thread_routine(void *args)
{
    int tcp_socket = *((int *)(args));
    size_t current_floor = 0;
    size_t target_floor = 0;
    uint8_t cab_buttons[FLOOR_COUNT] = {0};
    enum ElevatorState current_state = ELEVATOR_STATE_IDLE;
    while (1)
    {
        for (size_t i = 0; i < FLOOR_COUNT; ++i)
        {
            uint8_t floor_state = 0;

            struct Message msg = {.command = ORDER_BUTTON, .args = {0, i}};
            send(tcp_socket, &msg, sizeof(msg), 0);
            recv(tcp_socket, &msg, sizeof(msg), 0);
            floor_state = floor_state | msg.args[0];
            msg.args[0] = 1;
            msg.args[1] = i;
            send(tcp_socket, &msg, sizeof(msg), 0);
            recv(tcp_socket, &msg, sizeof(msg), 0);
            floor_state = floor_state | (msg.args[0] << 1);

            pthread_mutex_lock(&context.lock);
            context.floor_states[i] |= floor_state;
            pthread_mutex_unlock(&context.lock);

            msg.command = ORDER_BUTTON;
            msg.args[0] = CAB;
            msg.args[1] = i;
            send(tcp_socket, &msg, sizeof(msg), 0);
            recv(tcp_socket, &msg, sizeof(msg), 0);
            if (msg.args[0])
            {
                cab_buttons[i] = 1;
            }
        }

        struct Message msg = {.command = FLOOR_SENSOR};
        send(tcp_socket, &msg, sizeof(msg), 0);
        recv(tcp_socket, &msg, sizeof(msg), 0);
        current_floor = msg.args[1];

        if (current_state == ELEVATOR_STATE_MOVING && current_floor == target_floor && msg.args[0])
        {
            send(tcp_socket, &msg_motor_stop, sizeof(msg_motor_stop), 0);
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
                if (target_floor > current_floor)
                {
                    send(tcp_socket, &msg_motor_up, sizeof(msg_motor_up), 0);
                }
                if (target_floor < current_floor)
                {
                    send(tcp_socket, &msg_motor_down, sizeof(msg_motor_down), 0);
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
                    send(tcp_socket, &msg_motor_up, sizeof(msg_motor_up), 0);
                }
                if (target_floor < current_floor)
                {
                    send(tcp_socket, &msg_motor_down, sizeof(msg_motor_down), 0);
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

    int sockets[ELEVATOR_COUNT];
    pthread_t pthread[ELEVATOR_COUNT];
    for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
    {

        sockets[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockets[i] < 0)
        {
            return sockets[i];
        }

        struct sockaddr *addr;

        struct sockaddr_in addr_in;
        addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr_in.sin_port = htons(15657 + i);
        addr_in.sin_family = AF_INET;

        addr = (struct sockaddr *)&addr_in;

        struct timeval time;
        time.tv_sec = UINT32_MAX;
        time.tv_usec = 0;

        int err = setsockopt(sockets[i], SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time));
        if (err)
        {
            return err;
        }

        err = connect(sockets[i], addr, sizeof(addr_in));
        if (err)
        {
            return err;
        }

        pthread_create(&pthread[i], NULL, thread_routine, &sockets[i]);
    }

    for (size_t i = 0; i < ELEVATOR_COUNT; ++i)
    {
        pthread_join(pthread[i], NULL);
    }

    return 0;
}
