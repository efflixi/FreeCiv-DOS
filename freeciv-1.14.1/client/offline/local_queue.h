#ifndef FC__LOCAL_QUEUE_H
#define FC__LOCAL_QUEUE_H

#include <stddef.h>

struct local_queue {
  unsigned char *data;
  size_t capacity;
  size_t head;
  size_t count;
};

int local_queue_init(struct local_queue *queue, size_t capacity);
void local_queue_free(struct local_queue *queue);
int local_queue_write(void *context, const unsigned char *data, int len);
size_t local_queue_read(struct local_queue *queue, unsigned char *data,
                       size_t capacity);

#endif
