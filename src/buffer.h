#ifndef BUFFER_H
#define BUFFER_H

#include <Arduino.h>

typedef struct {
  uint16_t rel_time;
  uint8_t channel;
  int16_t value;
} buffer_item_t;

#define BUFFER_SIZE 16384
#define BUFFER_SIZE_MASK (BUFFER_SIZE - 1)

extern buffer_item_t ringbuffer[];
extern uint16_t ringbuffer_head;

void dumpBuffer();

#endif
