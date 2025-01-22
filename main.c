#include <log.h>
#include <netinet/ip.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

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

enum FloorFlags{
    FLOOR_FLAG_BUTTON_UP = 0x1,
    FLOOR_FLAG_BUTTON_DOWN = 0x2
};

static struct{
    uint8_t floor_states[FLOOR_COUNT];
    pthread_mutex_t lock;
} context;


static const struct Message msg_motor_up = {.command = MOTOR_DIRECTION, .args = {1}};
static const struct Message msg_motor_down = {.command = MOTOR_DIRECTION, .args = {-1}};
static const struct Message msg_motor_stop = {.command = MOTOR_DIRECTION, .args = {0}};


void *thread_routine(void *args){
    int tcp_socket = *((int *)(args));
    while(1){
        for(size_t i = 0; i<FLOOR_COUNT; ++i){
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
        }

        pthread_mutex_lock(&context.lock);
        bool floor_found = false;
        for(size_t i = 0; i<FLOOR_COUNT; ++i){
            if(context.floor_states[i] & (FLOOR_FLAG_BUTTON_DOWN | FLOOR_FLAG_BUTTON_UP)){
                pthread_mutex_unlock(&context.lock);
                floor_found = true;
                struct Message msg = {.command = FLOOR_SENSOR, .args = {0}};
                send(tcp_socket, &msg, sizeof(msg), 0);
                recv(tcp_socket, &msg, sizeof(msg), 0);
                size_t current_floor = msg.args[1];
                size_t target_floor = i;
                if(target_floor > current_floor){
                    send(tcp_socket, &msg_motor_up, sizeof(msg_motor_up), 0);
                }
                if(target_floor < current_floor){
                    send(tcp_socket, &msg_motor_down, sizeof(msg_motor_down), 0);
                }
                if(target_floor == current_floor){
                    pthread_mutex_lock(&context.lock);
                    context.floor_states[target_floor] = 0;
                    pthread_mutex_unlock(&context.lock);
                    break;
                }
                while(target_floor != current_floor){
                    send(tcp_socket, &msg, sizeof(msg), 0);
                    recv(tcp_socket, &msg, sizeof(msg), 0);
                    if(msg.args[0] == 1){
                        current_floor = msg.args[1];
                    }
                }
                send(tcp_socket, &msg_motor_stop, sizeof(msg_motor_stop), 0);
                break;
            }
        }
        if(floor_found == false){
            pthread_mutex_unlock(&context.lock);
        }
    }
    
}


int main()
{
    pthread_mutex_init(&context.lock, NULL);
    pthread_mutex_lock(&context.lock);
    for(size_t i = 0; i<FLOOR_COUNT; ++i){
        context.floor_states[i] = 0;
    }
    pthread_mutex_unlock(&context.lock);

    int sockets[ELEVATOR_COUNT];
    for(size_t i = 0; i<ELEVATOR_COUNT; ++i){

        sockets[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockets[i] < 0)
        {
            return sockets[i];
        }

        struct sockaddr *addr;

        struct sockaddr_in addr_in;
        addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr_in.sin_port = htons(15657);
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
        
        pthread_t pthread;
        pthread_create(&pthread, NULL, thread_routine, sockets + i);
        pthread_join(pthread, NULL);
    }

    return 0;
}
