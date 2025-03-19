#include <elevator.h>
#include <errno.h>
#include <fcntl.h>
#include <log.h>
#include <process.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct
{
    sem_t primary_sem;
    sem_t backup_sem;
    system_state_t state;
} shared_memory_t;

static shared_memory_t *shared_memory;

static void *signal_primary_routine(void *arg)
{
    sem_t *incrementing_sem = &shared_memory->primary_sem;
    sem_t *decrementing_sem = &shared_memory->backup_sem;

    char command[512];
    (void)snprintf(command, sizeof(command), "gnome-terminal -- bash -c \"./master -i %d -b 1; exec bash\"",
                   (uint16_t)(*((uint8_t *)arg)));

    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    time.tv_sec += 4;
    while (1)
    {
        sleep(5);
        if (sem_timedwait(decrementing_sem, &time) == 0)
        {
            sem_post(incrementing_sem);
        }
        else
        {
            LOG_INFO("Starting backup process");
            sleep(10);
            system(command);
        }
    }

    return NULL;
}

static void *signal_backup_routine(void *arg)
{
    sem_t *decrementing_sem = &shared_memory->primary_sem;
    sem_t *incrementing_sem = &shared_memory->backup_sem;

    char command[512];
    (void)snprintf(command, sizeof(command), "gnome-terminal -- bash -c \"./master -i %d -b 0; exec bash\"",
                   (uint16_t)(*((uint8_t *)arg)));

    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    time.tv_sec += 4;
    while (1)
    {
        sleep(5);
        if (sem_timedwait(decrementing_sem, &time) == 0)
        {
            sem_post(incrementing_sem);
        }
        else
        {
            LOG_INFO("Starting main process\n");
            sleep(10);
            system(command);
        }
    }

    return NULL;
}

int process_init(uint8_t is_primary, uint8_t index)
{
    char file_name[7] = {index + 'A', '.', 't', 'e', 'm', 'p', '\0'};

    int fd = shm_open(file_name, O_CREAT | O_EXCL | O_RDWR, 0660);

    if (fd == -1)
    {
        fd = shm_open(file_name, O_RDWR, 0660);
        LOG_INFO("Shared file already exists\n");
    }
    ftruncate(fd, sizeof(shared_memory_t));
    shared_memory = mmap(NULL, sizeof(*shared_memory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if ((void *)shared_memory == MAP_FAILED)
    {
        LOG_ERROR("mmap failed, err = %d\n", errno);
        return -errno;
    }

    if (sem_trywait(&shared_memory->primary_sem) == -1 && errno == EINVAL)
    {
        if (sem_init(&shared_memory->primary_sem, 1, 1) == -1)
        {
            LOG_ERROR("sem_init failed, err = %d\n", errno);
            return -errno;
        }
        if (sem_init(&shared_memory->backup_sem, 1, 0) == -1)
        {
            LOG_ERROR("sem_init failed, err = %d\n", errno);
            return -errno;
        }
        LOG_INFO("Initialized semaphores");
    }
    else
    {
        sem_post(&shared_memory->primary_sem);
    }

    uint16_t ports[ELEVATOR_COUNT] = {10042, 10043, 10044};
    pthread_t thread;
    if (is_primary)
    {
        struct sockaddr_in addr_in = {
            .sin_addr.s_addr = htonl(INADDR_ANY), .sin_port = htons(ports[index]), .sin_family = AF_INET};

        int err = 0;
        socklen_t error_code_size = sizeof(err);
        int retval = getsockopt(shared_memory->state.peer_socket, SOL_SOCKET, SO_ERROR, &err, &error_code_size);

        if (err != 0 || retval == -1)
        {
            LOG_INFO("Initializing peer socket\n");
            shared_memory->state.peer_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (shared_memory->state.peer_socket == -1)
            {
                LOG_ERROR("socket init error = %d\n", errno);
            }

            int value = 1;
            if (setsockopt(shared_memory->state.peer_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) == -1)
            {
                LOG_ERROR("Set reusable error = %d\n", errno);
                (void)close(shared_memory->state.peer_socket);
            }
            if (setsockopt(shared_memory->state.peer_socket, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value)) == -1)
            {
                LOG_ERROR("Set broadcast error = %d\n", errno);
                (void)close(shared_memory->state.peer_socket);
            }

            struct timeval time = {.tv_usec = 1, .tv_sec = 0};
            if (setsockopt(shared_memory->state.peer_socket, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) == -1)
            {
                LOG_ERROR("Set timeout error = %d\n", errno);
                (void)close(shared_memory->state.peer_socket);
            }

            if (bind(shared_memory->state.peer_socket, (struct sockaddr *)&addr_in, sizeof(addr_in)) == -1)
            {
                LOG_ERROR("Bind failed error = %d\n", errno);
                (void)close(shared_memory->state.peer_socket);
            }
        }

        addr_in.sin_port = htons(15657 + index);
        shared_memory->state.elevator_socket = elevator_init(&addr_in);

        pthread_create(&thread, NULL, signal_primary_routine, &index);
    }
    else
    {
        pthread_create(&thread, NULL, signal_backup_routine, &index);
        pthread_join(thread, NULL);
        return 0;
    }

    elevator_run(&shared_memory->state, ports, index);

    return 0;
}