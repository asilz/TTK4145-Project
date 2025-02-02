#include <netinet/ip.h>
#include <sys/socket.h>

struct Socket;

typedef enum CommandType
{
    COMMAND_TYPE_RELOAD_CONFIG = 0,
    COMMAND_TYPE_MOTOR_DIRECTION,
    COMMAND_TYPE_ORDER_BUTTON_LIGHT,
    COMMAND_TYPE_FLOOR_INDICATOR,
    COMMAND_TYPE_DOOR_OPEN_LIGHT,
    COMMAND_TYPE_STOP_BUTTON_LIGHT,
    COMMAND_TYPE_ORDER_BUTTON,
    COMMAND_TYPE_FLOOR_SENSOR,
    COMMAND_TYPE_STOP_BUTTON,
    COMMAND_TYPE_OBSTRUCTION_SWITCH,
} CommandType;

typedef enum ButtonType
{
    BUTTON_TYPE_HALL_UP,
    BUTTON_TYPE_HALL_DOWN,
    BUTTON_TYPE_CAB,
} ButtonType;

struct Message
{
    int8_t command;
    int8_t args[3];
};

typedef int (*send_func)(struct Socket *sock, const struct Message *msg);
typedef int (*receive_func)(struct Socket *sock, struct Message *msg);

typedef struct Socket
{
    int socket;
    struct sockaddr address;
    send_func send;
    receive_func recv;
} Socket;

int socket_udp_init(Socket *sock, struct sockaddr_in *address, struct sockaddr_in *bind_address);
int socket_tcp_client_init(Socket *sock, struct sockaddr_in *address);
int socket_tcp_server_init(Socket *sock, struct sockaddr_in *address);