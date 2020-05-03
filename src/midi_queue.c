#include <midi_queue.h>

void midi_queue_init(
    struct midi_queue *queue, struct midi_event *buffer, size_t size) {
  queue->buffer = buffer;
  queue->size = size;
  queue->head = 0;
  queue->tail = 0;
}

void midi_queue_push(struct midi_queue *queue, struct midi_event *event) {
  queue->buffer[queue->tail++] = *event;
  if (queue->tail == queue->size)
    queue->tail = 0;

  if (queue->tail == queue->head) {
    if (++queue->head == queue->size)
      queue->head = 0;
  }
}

struct midi_event midi_queue_pop(struct midi_queue *queue) {
  size_t prev_head = queue->head++;
  if (queue->head == queue->size)
    queue->head = 0;
  return queue->buffer[prev_head];
}

bool midi_queue_empty(struct midi_queue *queue) {
  return queue->head == queue->tail;
}
