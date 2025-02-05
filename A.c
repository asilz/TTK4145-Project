#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <semaphore.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int main(void)
{
    int fd = shm_open("c.temp", O_CREAT | O_EXCL | O_RDWR, 0660);

    if (fd == -1)
    {
        fd = shm_open("c.temp", O_RDWR, 0660);
        printf("%d\n", errno);
    }
    ftruncate(fd, 2 * sizeof(sem_t) + sizeof(size_t));
    sem_t *semaphores = mmap(NULL, 2 * sizeof(sem_t) + sizeof(size_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if ((void *)semaphores == MAP_FAILED)
    {
        printf("mmap failed, err = %d\n", errno);
    }
    uint8_t *data = (uint8_t *)semaphores;
    for (size_t i = 0; i < 2 * sizeof(sem_t) + sizeof(size_t); ++i)
    {
        printf("%d", data[i]);
    }
    printf("\n");
    if (sem_trywait(semaphores) == -1 && errno == EINVAL)
    {
        sem_init(&semaphores[0], 1, 1);
        sem_init(&semaphores[1], 1, 0);
        size_t *count = (size_t *)(semaphores + 2);
        *count = 0;
        printf("semaphores initialized\n");
    }
    else
    {
        if (sem_post(semaphores) == -1)
        {
            printf("sem post err = %d\n", errno);
        }
    }

    sem_t *incrementing = semaphores;
    sem_t *decrementing = semaphores + 1;
    uint8_t active = 1;
    while (1)
    {
        sleep(1);
        struct timespec time;
        clock_gettime(CLOCK_REALTIME, &time);
        time.tv_sec += 1;
        if (sem_timedwait(decrementing, &time) == 0)
        {
            sem_post(incrementing);
            printf("pid = %d\n", getpid());
            if (active)
            {
                size_t *count = (size_t *)(semaphores + 2);
                printf("counting = %zu\n", (*count)++);
            }
        }
        else
        {
            printf("timed wait err = %d\n", errno);
            active = 0;
            incrementing = semaphores + 1;
            decrementing = semaphores;
            if (sem_post(incrementing) == -1)
            {
                printf("sem post err = %d\n", errno);
            }
            printf("cmd\n");
            sleep(2);
            const char *cmd = "gnome-terminal -- bash -c \"./a; exec bash\"";
            system(cmd);
            // execlp("x-terminal-emulator", "x-terminal-emulator", "-e", "./home/student/TTK4145-Project/build/a",
            // NULL);
            /*
            printf("forking\n");
            sleep(5);
            if ((pid = fork()) == 0)
            {

                int err = execl("./a", "a", NULL);
            }
            */
        }
    }

    return 0;

    int pid = getppid();

restart:
    if (pid == 1)
    {
        if ((pid = fork()) == 0)
        {
            printf("forking\n");
            sleep(10);
            int err = execl("./a", "a", NULL);
        }
        else
        {
            printf("failed to fork\n");
            // munmap((void*)memory, 8);
        }
    }
    while (1)
    {
        printf("%d\n", getpid());
        if (kill(pid, 0))
        {
            goto restart;
        }
        sleep(15);
    }
}

void sleep_litt() { sleep(1); }