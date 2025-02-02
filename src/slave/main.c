
#include <log.h>
#include <network.h>

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

typedef enum SlaveState
{
    SLAVE_STATE_CONNECTED = 0,
    SLAVE_STATE_DISCONNECTED = 1
} SlaveState;

int main(void)
{

    char cab_calls[FLOOR_COUNT];
    Socket elevator_sock;
    Socket master_sock;

    struct sockaddr_in address;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(MASTER_PORT);
    address.sin_family = AF_INET;

    struct sockaddr_in bind_address = {
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK), .sin_port = htons(SLAVE_PORT), .sin_family = AF_INET};

    socket_udp_init(&master_sock, &address, &bind_address);

    address.sin_port = htons(15657);
    socket_tcp_client_init(&elevator_sock, &address);

    SlaveState state = SLAVE_STATE_CONNECTED;

    while (1)
    {
        while (state == SLAVE_STATE_CONNECTED)
        {
            int err = 0;
            for (size_t i = 0; i < FLOOR_COUNT; ++i)
            {
                struct Message msg = {.command = COMMAND_TYPE_ORDER_BUTTON, .args = {BUTTON_TYPE_CAB, i}};
                err = elevator_sock.send(&elevator_sock, &msg);
                if (err != 0)
                {
                    LOG_ERROR("err = %d", err);
                }
                err = elevator_sock.recv(&elevator_sock, &msg);
                if (err != 0)
                {
                    LOG_ERROR("err = %d", err);
                }
                cab_calls[i] |= msg.args[0];
            }

            struct Message msg;
            err = master_sock.recv(&master_sock, &msg);
            if (err != 0)
            {
                LOG_ERROR("err = %d", err);
            }
            if (err == 0)
            {
                if (msg.command == COMMAND_TYPE_ORDER_BUTTON &&
                    (msg.args[0] == BUTTON_TYPE_HALL_UP || msg.args[0] == BUTTON_TYPE_HALL_DOWN))
                {

                    err = elevator_sock.send(&elevator_sock, &msg);
                    if (err != 0)
                    {
                        LOG_ERROR("err = %d", err);
                    }
                    err = elevator_sock.recv(&elevator_sock, &msg);
                    if (err != 0)
                    {
                        LOG_ERROR("err = %d", err);
                    }
                    if (msg.args[0] == 1)
                    {
                        LOG_INFO("HALL BUTTON\n");
                    }
                    err = master_sock.send(&master_sock, &msg);
                    if (err != 0)
                    {
                        LOG_ERROR("err = %d", err);
                    }
                }
                else if (msg.command == COMMAND_TYPE_ORDER_BUTTON && msg.args[0] == BUTTON_TYPE_CAB)
                {
                    msg.args[0] = cab_calls[msg.args[1]];
                    err = master_sock.send(&master_sock, &msg);
                    if (err != 0)
                    {
                        LOG_ERROR("err = %d", err);
                    }
                }
                else if (msg.command > 5)
                {
                    err = elevator_sock.send(&elevator_sock, &msg);
                    if (err != 0)
                    {
                        LOG_ERROR("err = %d\n", err);
                    }
                    err = elevator_sock.recv(&elevator_sock, &msg);
                    if (err != 0)
                    {
                        LOG_ERROR("err = %d\n", err);
                    }
                    err = master_sock.send(&master_sock, &msg);
                    if (err != 0)
                    {
                        LOG_ERROR("err = %d\n", err);
                    }
                }
                else
                {
                    err = elevator_sock.send(&elevator_sock, &msg);
                    if (err != 0)
                    {
                        LOG_ERROR("err = %d\n", err);
                    }
                }
            }
        }
        while (state == SLAVE_STATE_DISCONNECTED)
        {
        }
    }

    return 0;
}