#ifndef CHANNEL_H
#define CHANNEL_H

#include <Arduino.h>
#include <math.h>

#define STATUS_LOOKING_FOR_ZERO 0
#define STATUS_SAMPLING 1
#define STATUS_DONE 2
#define STATUS_DONE_WITH_ERROR 3

#define HALF_PERIOD_BY_BATCH 100

typedef enum {
  VOLTAGE,
  CURRENT
} channel_kind_t;

typedef struct {
  uint8_t channel;
  channel_kind_t kind;
  float_t calibratedRatio;
  int16_t value;
  uint32_t adc_precalculated_input;
  int16_t dynamic_offset;
  int16_t authorized_additional_offset;
  int8_t deltaToVoltage;

  uint8_t status;
  uint32_t numSamples;
  int64_t sumSamples;
  uint64_t sumSquaresSamples;
  uint64_t sumUISamples;
  uint16_t sameSignNumSamples;
  int8_t lastSign;
  uint8_t numHalfPeriod;
  uint16_t startIndex;
  uint32_t firstCCountDeltaOfBatch;
  uint32_t sumCCountDeltaOfBatch;
  uint32_t startOfBatchTime;
  uint32_t endOfBatchTime;
  uint32_t startOfBatchCCount;
  uint32_t endOfBatchCCount;
  uint8_t previousBatchInError;
  uint32_t lastSampleCCount;
  float_t lastVrms;
  float_t lastVavg;
  double_t activeEnergyIndex;
  double_t reactiveEnergyIndex;
  uint32_t lastEnergyIndexTime;
} channel_t;

#define CHANNELS 7
extern channel_t channels[];
extern channel_t channel_standard_voltage;

void setup(channel_t* channel, uint8_t channel_number, float_t calibratedRatio, channel_kind_t kind = VOLTAGE, int8_t delta_to_voltage = 0);
void process(channel_t* channel);
void processBatch(channel_t* channel);

#endif
