#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    int pid;
    if ((pid = fork()) == 0)
    {
        int err = execl("./a", "a", NULL);
    }
    else
    {
        int fd = shm_open("a.temp", O_CREAT | O_EXCL | O_RDWR, 0660);

        if (fd == -1)
        {
            fd = shm_open("a.temp", O_RDWR, 0660);
            printf("%d\n", errno);
        }

        uint64_t var = 0xDEADBEEF;
        write(fd, &var, sizeof(var));
        lseek(fd, -8, SEEK_CUR);

        wait(NULL);
        // munmap((void*)memory, 8);
    }
}