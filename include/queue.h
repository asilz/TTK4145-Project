#include <stddef.h>

struct Order
{
    size_t floor;
};

int order_queue_init();
int order_queue_push(const struct Order *order);
int order_queue_pop(struct Order *order);
int order_queue_deinit();