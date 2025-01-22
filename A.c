#include <sys/shm.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

int main(void){
    int fd = shm_open("a.temp", O_CREAT | O_EXCL | O_RDWR, 0660);
    
    if(fd == -1){
        fd = shm_open("a.temp",O_RDWR, 0660);
        printf("%d\n", errno);
    }
    void *memory = mmap(NULL, 8, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
    *((uint64_t *)memory) = 0xF00DBEEF;

    sleep(3);

    
    
    for(size_t i = 0; i<10; ++i){
        printf("%lX" "\n", *((uint64_t *)memory));
    }

    //munmap()
    
    

    return 4;
}