#pragma once

#include <math.h>

#define STATUS_LOOKING_FOR_ZERO 0
#define STATUS_SAMPLING 1
#define STATUS_DONE 2
#define STATUS_DONE_WITH_ERROR 3

#define MEASURE_STATE_UNKNOWN 0
#define MEASURE_STATE_IDLE 1
#define MEASURE_STATE_OPEN 2
#define MEASURE_STATE_ACTIVE 3
#define MEASURE_STATE_ERROR 4

#define HALF_PERIOD_BY_BATCH 100

#define CHANNELS 7


typedef enum {
  VOLTAGE,
  CURRENT
} channel_kind_t;

typedef struct channel_calibration_t
{
  float_t CT_amp_per_volt;
  uint8_t CT_loops;
  float_t ratio;
} channel_calibration_t;

typedef struct channel_info_t
{
  uint8_t channel;
  channel_kind_t kind;
  float_t calibratedRatio;  
  int8_t deltaToVoltage;
  uint32_t adc_precalculated_input;
  uint32_t batch_counter;
} channel_info_t;

typedef struct channel_global_t
{
  float_t lastVrms;
  float_t lastVavg;
  int16_t dynamic_offset;
  int16_t authorized_additional_offset;
  uint8_t previousBatchInError;
} channel_global_t;

typedef struct sample_t
{
  int16_t value;
  uint16_t raw_value;
  int8_t lastSign;
} sample_t;

typedef struct channel_batch_t
{
  uint32_t batchCounter;
  uint8_t batchNumber;
  uint8_t status;
  uint32_t numSamples;
  int64_t sumSamples;
  int64_t sumRawSamples;
  uint64_t sumSquaresSamples;
  int64_t sumUISamples;
  uint32_t startOfBatchTime;
  uint32_t endOfBatchTime;
  uint32_t startOfBatchCCount;
  uint32_t endOfBatchCCount;
  uint32_t firstCCountDeltaOfBatch;
  uint32_t sumCCountDeltaOfBatch;

  uint16_t sameSignNumSamples;
  uint8_t numHalfPeriod;
  uint16_t startIndex;
  uint32_t lastSampleCCount;

} channel_batch_t;

typedef struct channel_index_t
{
  double_t activeEnergyIndex;
  double_t reactiveEnergyIndex;
  uint32_t lastEnergyIndexTime;
} channel_index_t;

typedef struct channel_measure_t
{
  uint8_t state;
  int32_t timestamp;
  float_t Vrms;
  float_t Irms;
  float_t Papp;
  float_t Preal;
  float_t power_factor;
  float_t phase;
} channel_measure_t;

typedef struct channel_t {

  channel_info_t info;
  channel_global_t global;

  sample_t sample;
  channel_batch_t batches[2];
  channel_index_t index;
  channel_measure_t measure;

  channel_batch_t *capturing_batch;
  channel_batch_t *processing_batch;
} channel_t;
