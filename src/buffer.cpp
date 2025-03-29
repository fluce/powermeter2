#include "buffer.h"

buffer_item_t ringbuffer[BUFFER_SIZE];
uint16_t ringbuffer_head = 0;

void dump_buffer() {
  Serial.println("Buffer full");
  for (int i = 0; i < BUFFER_SIZE; i++) {
    Serial.printf("%d %d %d %d\r\n", i, ringbuffer[i].rel_time, ringbuffer[i].channel, ringbuffer[i].value);
  }
}