#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "local_queue.h"

int local_queue_init(struct local_queue *queue, size_t capacity)
{
  if (!queue || queue->data || capacity == 0 || capacity > INT_MAX) {
    fprintf(stderr, "Offline transport: invalid queue initialization.\n");
    return -1;
  }
  queue->data = malloc(capacity);
  if (!queue->data) {
    fprintf(stderr, "Offline transport: queue allocation failed.\n");
    return -1;
  }
  queue->capacity = capacity;
  queue->head = 0;
  queue->count = 0;
  return 0;
}

void local_queue_free(struct local_queue *queue)
{
  if (queue) {
    free(queue->data);
    memset(queue, 0, sizeof(*queue));
  }
}

int local_queue_write(void *context, const unsigned char *data, int len)
{
  struct local_queue *queue = context;
  size_t tail, first;

  if (!queue || !queue->data || !data || len <= 0) {
    fprintf(stderr, "Offline transport: invalid queue write.\n");
    return -1;
  }
  if ((size_t)len > queue->capacity - queue->count) {
    return 0;
  }
  tail = (queue->head + queue->count) % queue->capacity;
  first = queue->capacity - tail;
  if (first > (size_t)len) {
    first = (size_t)len;
  }
  memcpy(queue->data + tail, data, first);
  memcpy(queue->data, data + first, (size_t)len - first);
  queue->count += (size_t)len;
  return len;
}

size_t local_queue_read(struct local_queue *queue, unsigned char *data,
                       size_t capacity)
{
  size_t count, first;

  if (!queue || !queue->data || !data || capacity == 0) {
    fprintf(stderr, "Offline transport: invalid queue read.\n");
    return 0;
  }
  count = queue->count < capacity ? queue->count : capacity;
  first = queue->capacity - queue->head;
  if (first > count) {
    first = count;
  }
  memcpy(data, queue->data + queue->head, first);
  memcpy(data + first, queue->data, count - first);
  queue->head = (queue->head + count) % queue->capacity;
  queue->count -= count;
  return count;
}
