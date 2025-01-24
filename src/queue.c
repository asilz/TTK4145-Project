#include <pthread.h>
#include <queue.h>
#include <semaphore.h>

#define QUEUE_SIZE 32

static struct OrderQueue
{
    struct Order data[QUEUE_SIZE];
    size_t head_index;
    size_t tail_index;

    sem_t full;
    sem_t empty;
    pthread_mutex_t mutex;
} order_queue;

int order_queue_init()
{
    sem_init(&order_queue.full, 0, 0);
    sem_init(&order_queue.empty, 0, (sizeof(order_queue.data) / sizeof(order_queue.data[0])));
    pthread_mutex_init(&order_queue.mutex, NULL);

    return 0;
}

int order_queue_push(const struct Order *order)
{
    sem_wait(&order_queue.empty);
    pthread_mutex_lock(&order_queue.mutex);
    order_queue.data[order_queue.tail_index++] = *order;
    order_queue.tail_index = order_queue.tail_index % (sizeof(order_queue.data) / sizeof(order_queue.data[0]));
    pthread_mutex_unlock(&order_queue.mutex);
    sem_post(&order_queue.full);

    return 0;
}

int order_queue_pop(struct Order *order)
{
    sem_wait(&order_queue.full);
    pthread_mutex_lock(&order_queue.mutex);
    *order = order_queue.data[order_queue.head_index++];
    order_queue.tail_index = order_queue.head_index % (sizeof(order_queue.data) / sizeof(order_queue.data[0]));
    pthread_mutex_unlock(&order_queue.mutex);
    sem_post(&order_queue.empty);

    return 0;
}

int order_queue_deinit()
{
    sem_destroy(&order_queue.full);
    sem_destroy(&order_queue.empty);
    pthread_mutex_destroy(&order_queue.mutex);

    return 0;
}
