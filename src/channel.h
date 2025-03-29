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


typedef struct channel_info_t
{
  uint8_t channel;
  channel_kind_t kind;
  float_t calibratedRatio;  
  int8_t deltaToVoltage;
} channel_info_t;

typedef struct channel_global_t
{
  float_t lastVrms;
  float_t lastVavg;
  int16_t dynamic_offset;
  int16_t authorized_additional_offset;
} channel_global_t;

typedef struct channel_cross_batch_t
{
  int16_t value;
} channel_cross_batch_t;

typedef struct channel_batch_t
{
  uint8_t status;
  uint8_t previousBatchInError;
  uint32_t numSamples;
  int64_t sumSamples;
  uint64_t sumSquaresSamples;
  uint64_t sumUISamples;
  uint32_t startOfBatchTime;
  uint32_t endOfBatchTime;
  uint32_t startOfBatchCCount;
  uint32_t endOfBatchCCount;
  uint32_t firstCCountDeltaOfBatch;
  uint32_t sumCCountDeltaOfBatch;

  uint16_t sameSignNumSamples;
  int8_t lastSign;
  uint8_t numHalfPeriod;
  uint16_t startIndex;
  uint32_t lastSampleCCount;

} channel_process_t;

typedef struct channel_index_t
{
  double_t activeEnergyIndex;
  double_t reactiveEnergyIndex;
  uint32_t lastEnergyIndexTime;
} channel_index_t;

typedef struct channel_t {

  channel_info_t info;
  channel_global_t global;

  uint32_t adc_precalculated_input;

  channel_cross_batch_t cross_batch;
  channel_batch_t batch;
  channel_index_t index;
} channel_t;

#define CHANNELS 7
extern channel_t channels[];
extern channel_t channel_standard_voltage;

void setup(channel_t* channel, uint8_t channel_number, float_t calibratedRatio, channel_kind_t kind = VOLTAGE, int8_t delta_to_voltage = 0);
void process(channel_info_t *info, channel_cross_batch_t *cross, channel_batch_t *channel);
void processBatch(channel_info_t *channel, channel_batch_t *channel_batch, channel_index_t *channel_index, channel_global_t *channel_global);

#endif
