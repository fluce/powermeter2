#ifndef CHANNEL_H
#define CHANNEL_H

#include <Arduino.h>
#include "types.h"

extern channel_t channels[];
extern channel_t channel_standard_voltage;

void setup(channel_t* channel, uint8_t channel_number, float_t calibratedRatio, channel_kind_t kind = VOLTAGE, int8_t delta_to_voltage = 0);
void process_sample(channel_info_t *info, sample_t *cross, channel_batch_t *channel);
void end_of_batch(channel_batch_t* batch);

#endif
