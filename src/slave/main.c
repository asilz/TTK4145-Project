
#include <driver.h>
#include <errno.h>
#include <log.h>
#include <string.h>
#include <sys/time.h>

#ifndef MASTER_PORT
#define MASTER_PORT 17532
#endif

#ifndef SLAVE_PORT
#define SLAVE_PORT 17533
#endif

typedef enum slave_state_e
{
    SLAVE_STATE_CONNECTED = 0,
    SLAVE_STATE_DISCONNECTED = 1
} slave_state_e;

int main(void)
{

    socket_t elevator_sock;
    socket_t master_sock;

    struct sockaddr_in address = {
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK), .sin_port = htons(MASTER_PORT), .sin_family = AF_INET};

    struct sockaddr_in bind_address = {
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK), .sin_port = htons(SLAVE_PORT), .sin_family = AF_INET};

    node_udp_init(&master_sock, &address, &bind_address);

    address.sin_port = htons(15657);
    elevator_init(&elevator_sock, &address);

    slave_state_e state = SLAVE_STATE_CONNECTED;

    while (1)
    {
        if (state == SLAVE_STATE_CONNECTED)
        {
            struct packet_t packet;

            while (socket_recv(&master_sock, &packet) != 0)
            {
            }
            socket_send_recv(&elevator_sock, &packet);
            socket_send(&master_sock, &packet);
        }
        if (state == SLAVE_STATE_DISCONNECTED)
        {
        }
    }

    return 0;
}