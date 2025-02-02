#include <errno.h>
#include <inttypes.h>
#include <network.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static int send_slave(struct Socket *sock, const struct Message *msg) { return 0; }
static int send_udp(struct Socket *sock, const struct Message *msg)
{
    if (sendto(sock->socket, msg, sizeof(*msg), MSG_NOSIGNAL, &sock->address, sizeof(sock->address)) == -1)
    {
        return errno;
    }
    return 0;
}
static int recv_udp(struct Socket *sock, struct Message *msg)
{
    socklen_t size;
    struct sockaddr addr = sock->address;
    if (recvfrom(sock->socket, msg, sizeof(*msg), MSG_NOSIGNAL, &addr, &size) == -1)
    {
        return errno;
    }
    return 0;
}

static int send_tcp(struct Socket *sock, const struct Message *msg)
{
    if (send(sock->socket, msg, sizeof(*msg), MSG_NOSIGNAL) == -1)
    {
        return errno;
    }
    return 0;
}
static int recv_tcp(struct Socket *sock, struct Message *msg)
{
    if (recv(sock->socket, msg, sizeof(*msg), MSG_NOSIGNAL) == -1)
    {
        return errno;
    }
    return 0;
}

int socket_udp_init(Socket *sock, struct sockaddr_in *address, struct sockaddr_in *bind_address)
{
    sock->address = *(struct sockaddr *)address;
    sock->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock->socket == -1)
    {
        return errno;
    }
    int value = 1;
    if (setsockopt(sock->socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) == -1)
    {
        int err = errno;
        (void)close(sock->socket);
        return err;
    }

    struct timeval time;
    time.tv_sec = 4;
    time.tv_usec = 0;
    if (setsockopt(sock->socket, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) == -1)
    {
        int err = errno;
        (void)close(sock->socket);
        return err;
    }

    if (bind(sock->socket, (struct sockaddr *)bind_address, sizeof(*bind_address)) == -1)
    {
        int err = errno;
        (void)close(sock->socket);
        return err;
    }

    sock->send = send_udp;
    sock->recv = recv_udp;

    return 0;
}
int socket_tcp_client_init(Socket *sock, struct sockaddr_in *address)
{
    sock->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock->socket == -1)
    {
        return errno;
    }

    struct timeval time;
    time.tv_sec = UINT32_MAX;
    time.tv_usec = 0;

    if (setsockopt(sock->socket, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) == -1)
    {
        int err = errno;
        (void)close(sock->socket);
        return err;
    }

    sock->address = *(struct sockaddr *)address;
    if (connect(sock->socket, &sock->address, sizeof(sock->address)) == -1)
    {
        int err = errno;
        (void)close(sock->socket);
        return err;
    }

    sock->send = send_tcp;
    sock->recv = recv_tcp;

    return 0;
}

int socket_tcp_server_init(Socket *sock, struct sockaddr_in *address)
{
    sock->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock->socket == -1)
    {
        return errno;
    }

    struct timeval time;
    time.tv_sec = UINT32_MAX;
    time.tv_usec = 0;

    if (setsockopt(sock->socket, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) == -1)
    {
        int err = errno;
        (void)close(sock->socket);
        return err;
    }

    sock->address = *(struct sockaddr *)address;
    if (accept(sock->socket, &sock->address, NULL) == -1)
    {
        int err = errno;
        (void)close(sock->socket);
        return err;
    }

    sock->send = send_tcp;
    sock->recv = recv_tcp;

    return 0;
}

int socket_udp_elevator_init(Socket *sock, struct sockaddr_in *address)
{
    sock->address = *(struct sockaddr *)address;
    return 0;
}
int socket_tcp_elevator_init(Socket *sock, struct sockaddr_in *address)
{
    sock->address = *(struct sockaddr *)address;
    return 0;
}