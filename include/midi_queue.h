#ifndef STM32_MIDI_PIANO_INCLUDE_MIDI_QUEUE_H_
#define STM32_MIDI_PIANO_INCLUDE_MIDI_QUEUE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct midi_event {
  enum { NOTE_ON, NOTE_OFF } type;
  uint8_t note;
  uint8_t velocity;
};

struct midi_queue {
  struct midi_event *buffer;
  size_t head;
  size_t tail;
  size_t size;
};

void midi_queue_init(
    struct midi_queue *queue, struct midi_event *buffer, size_t size);
void midi_queue_push(struct midi_queue *queue, struct midi_event *event);
struct midi_event midi_queue_pop(struct midi_queue *queue);
bool midi_queue_empty(struct midi_queue *queue);

#endif  // STM32_MIDI_PIANO_INCLUDE_MIDI_QUEUE_H_
