#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>

#define COMMAND_TYPE_MOTOR_DIRECTION 1

typedef enum CommandType{
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

typedef enum ButtonType{
    HALL_UP,
    HALL_DOWN,
    CAB,
} ButtonType;



struct message {
    int8_t command;
    int8_t args[3];
};

#define FLOOR_COUNT 4


int main(){
    //elevio_init();
    printf("init");

    int tcp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(tcp_socket < 0){
        return tcp_socket;
    }

    printf("init2");

    struct sockaddr *addr;
    
    struct sockaddr_in addr_in;
    addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr_in.sin_port = htons(15657);
    addr_in.sin_family = AF_INET;

    addr = (struct sockaddr *) &addr_in;

    printf("28");
    
    struct timeval time;
    time.tv_sec = UINT32_MAX;
    time.tv_usec = 0;

    int err = setsockopt(tcp_socket, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time));
    if(err){
        return err;
    }

    err = connect(tcp_socket, addr, sizeof(addr_in));
    if(err){
        return err;
    }

    struct message msg = {.command = 0, .args = {0}};


    int size_socklen = sizeof(addr_in);
    
    uint8_t bitmap = 0;
    while(1){
        printf("Floor sensor\n");
        msg.command = FLOOR_SENSOR;
        err = sendto(tcp_socket, &msg, sizeof(msg), 0, addr, sizeof(addr_in));
        if(err){
            return err;
        }
        msg.command = 0;
        err = recvfrom(tcp_socket, &msg, sizeof(msg), 0, addr, &size_socklen);
        if(err){
            return err;
        }
        size_t current_floor = msg.args[1] - 1;

       

        size_t target_floor = current_floor;
        for(size_t i = 0; i<FLOOR_COUNT; ++i){
            printf("search floor\n");
            for(size_t j = 0; j<2; ++j){
                msg.command = ORDER_BUTTON;
                msg.args[0] = j;
                msg.args[1] = i;
                sendto(tcp_socket, &msg, sizeof(msg), 0, addr, sizeof(addr_in));
                recvfrom(tcp_socket, &msg, sizeof(msg), 0, addr, &size_socklen);

                if(msg.args[0] == 1){
                    bitmap |= 1 << i;
                }
            }
        }

        uint8_t bit = bitmap;
        for(size_t i = 0; i<FLOOR_COUNT;++i){
            if(bit & 1){
                target_floor = i;
                bitmap &= ~(1 << i);
                break;
            }
            bit = bit >> 1;
        }
        
        ssize_t direction = ((ssize_t)target_floor -  (ssize_t)current_floor);
        if(direction >0){
            direction = 1;
        }
        if(direction < 0){
            direction = -1;
        }
        printf("Motor direction\n");

        msg.command = MOTOR_DIRECTION;
        msg.args[0] = direction;
        sendto(tcp_socket, &msg, sizeof(msg), 0, addr, sizeof(addr_in));

        while(current_floor != target_floor){
            msg.command = FLOOR_SENSOR;
            sendto(tcp_socket, &msg, sizeof(msg), 0, addr, sizeof(addr_in));
            recvfrom(tcp_socket, &msg, sizeof(msg), 0, addr, &size_socklen);
            if(1 || msg.args[0]){
                size_t current_floor = msg.args[1] - 1;
            }
        }
        

        
        //

        //printf("%s",buf);
       
    }

    return 0;
}
