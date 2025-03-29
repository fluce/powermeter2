#include "channel.h"
#include "buffer.h"

typedef union input_u
{
  struct
  {
    uint32_t filler2 : 22;
    uint32_t channel : 3;
    uint32_t single_diff_flag : 1;
    uint32_t start : 1;
    uint32_t filler : 5;
  } input;
  uint32_t val;
} input;

input_u input_array[8] = {
    {.input = {.channel = 0, .single_diff_flag = 1, .start = 1}},
    {.input = {.channel = 1, .single_diff_flag = 1, .start = 1}},
    {.input = {.channel = 2, .single_diff_flag = 1, .start = 1}},
    {.input = {.channel = 3, .single_diff_flag = 1, .start = 1}},
    {.input = {.channel = 4, .single_diff_flag = 1, .start = 1}},
    {.input = {.channel = 5, .single_diff_flag = 1, .start = 1}},
    {.input = {.channel = 6, .single_diff_flag = 1, .start = 1}},
    {.input = {.channel = 7, .single_diff_flag = 1, .start = 1}},
};

uint32_t reverse(uint32_t val)
{
  uint8_t *t = (uint8_t *)&val;
  return (((((t[0] << 8) | t[1]) << 8) | t[2]) << 8) | t[3];
}

channel_t channel_standard_voltage;

channel_t channels[CHANNELS];

void setup(channel_t *channel, uint8_t channel_number, float_t calibratedRatio, channel_kind_t kind, int8_t delta_to_voltage)
{
  channel->info.channel = channel_number;
  channel->info.kind = kind;
  channel->cross_batch.value = 0;
  channel->adc_precalculated_input = reverse(input_array[channel->info.channel].val);
  channel->info.calibratedRatio = calibratedRatio;
  channel->global.dynamic_offset = 0x7fff;
  channel->global.authorized_additional_offset = 1024;
  channel->info.deltaToVoltage = delta_to_voltage;
}

#define XTHAL_GET_CCOUNT() ({ int __ccount; \
    __asm__ __volatile__("rsr.ccount %0" : "=a"(__ccount)); \
    __ccount; })

void process_complete(channel_info_t *info, channel_cross_batch_t *cross, channel_batch_t *channel)
{
  static uint32_t last_ccount = 0;
  uint32_t ccount = XTHAL_GET_CCOUNT();
  uint32_t rel_time = channel->lastSampleCCount == 0 ? 0 : ccount - channel->lastSampleCCount;
  channel->lastSampleCCount = ccount;

  ringbuffer[ringbuffer_head].rel_time = last_ccount == 0 ? 0 : ccount - last_ccount;
  last_ccount = ccount;
  ringbuffer[ringbuffer_head].channel = info->channel;
  ringbuffer[ringbuffer_head].value = cross->value;

  if (channel->firstCCountDeltaOfBatch == 0)
  {
    channel->firstCCountDeltaOfBatch = rel_time;
    channel->sumCCountDeltaOfBatch = 0;
  }
  else
  {
    channel->sumCCountDeltaOfBatch += rel_time;
  }
  int8_t sign = cross->value >= 0 ? 1 : -1;
  if (sign == channel->lastSign)
  {
    channel->sameSignNumSamples++;
  }
  else
  {
    uint32_t tmr = micros();
    if (channel->sameSignNumSamples > 5 && channel->status == STATUS_LOOKING_FOR_ZERO)
    {
      // Serial.printf("%d Channel=%d Start sampling\n", tmr, channel->channel);
      // Serial.flush();
      channel->status = STATUS_SAMPLING;
      channel->startIndex = ringbuffer_head;
      channel->numSamples = 0;
      channel->sumSamples = 0;
      channel->sumSquaresSamples = 0;
      channel->sumUISamples = 0;
      channel->numHalfPeriod = 0;
      channel->startOfBatchTime = tmr;
      channel->startOfBatchCCount = ccount;
    }
    channel->sameSignNumSamples = 1;
    channel->lastSign = sign;
    channel->numHalfPeriod++;
    uint32_t batchDuration = (tmr - channel->startOfBatchTime) / 1000;
    if (channel->numHalfPeriod == HALF_PERIOD_BY_BATCH)
    {
      // Serial.printf("Batch duration : %d\n", batchDuration);
      channel->endOfBatchTime = tmr;
      channel->endOfBatchCCount = ccount;
      if (batchDuration > 980 && batchDuration < 1010)
      {
        channel->status = STATUS_DONE;
      }
      else
      {
        channel->status = STATUS_DONE_WITH_ERROR;
      }
    }
    else if (batchDuration > 1200)
    {
      channel->status = STATUS_DONE_WITH_ERROR;
    }
  }
  if (channel->status == STATUS_SAMPLING)
  {
    channel->numSamples++;
    channel->sumSamples += cross->value;
    channel->sumSquaresSamples += cross->value * cross->value;
    if (info->kind == CURRENT)
    {
      int16_t current = cross->value;
      int16_t voltage = ringbuffer[(ringbuffer_head + info->deltaToVoltage) & BUFFER_SIZE_MASK].value;
      channel->sumUISamples += current * voltage;      
    }
  }
  ringbuffer_head = (ringbuffer_head + 1) & BUFFER_SIZE_MASK;
}

void process_simple(channel_info_t *info, channel_cross_batch_t *cross, channel_batch_t *channel)
{
  static uint32_t last_ccount = 0;
  uint32_t ccount = XTHAL_GET_CCOUNT();
  uint32_t rel_time = channel->lastSampleCCount == 0 ? 0 : ccount - channel->lastSampleCCount;
  channel->lastSampleCCount = ccount;

  ringbuffer[ringbuffer_head].rel_time = last_ccount == 0 ? 0 : ccount - last_ccount;
  last_ccount = ccount;
  ringbuffer[ringbuffer_head].channel = info->channel;
  ringbuffer[ringbuffer_head].value = cross->value;

  if (channel->status == STATUS_LOOKING_FOR_ZERO)
  {
    uint32_t tmr = micros();
    channel->status = STATUS_SAMPLING;
    channel->startIndex = ringbuffer_head;
    channel->numSamples = 0;
    channel->sumSamples = 0;
    channel->sumSquaresSamples = 0;
    channel->sumUISamples = 0;
    channel->numHalfPeriod = 0;
    channel->startOfBatchTime = tmr;
    channel->startOfBatchCCount = ccount;
  }
  if (channel->status == STATUS_SAMPLING)
  {
    channel->numSamples++;
    channel->sumSamples += cross->value;
    channel->sumSquaresSamples += cross->value * cross->value;
    if (info->kind == CURRENT)
    {
      int16_t current = cross->value;
      int16_t voltage = ringbuffer[(ringbuffer_head + info->deltaToVoltage) & BUFFER_SIZE_MASK].value;
      channel->sumUISamples += current * voltage;      
    }
  }
  ringbuffer_head = (ringbuffer_head + 1) & BUFFER_SIZE_MASK;
}

void process(channel_info_t *info, channel_cross_batch_t *cross, channel_batch_t *channel)
{
  if (info->kind == CURRENT)
  {
    process_simple(info,cross,channel);
  }
  else
  {
    process_complete(info,cross,channel);
  }
}

#define CALIBRATED_TIMER_FREQ 79200
uint32_t calculateBatchTimeDurationMs(channel_batch_t *channel_batch)
{
  return (channel_batch->endOfBatchTime - channel_batch->startOfBatchTime) / 1000;
}

void processBatch(channel_info_t *channel, channel_batch_t *channel_batch, channel_index_t *channel_index, channel_global_t *channel_global)
{
  // Serial.printf("Batch done : processing %d samples (status: %d)\n", channel->numSamples, channel->status);
  if (channel_batch->status == STATUS_DONE_WITH_ERROR)
  {
    if (!channel_batch->previousBatchInError)
    {
      channel_batch->previousBatchInError = 1;
      Serial.printf("Channel=%d\tTime=%d\tNoSignal\r\n", channel->channel, calculateBatchTimeDurationMs(channel_batch));
      Serial.flush();
    }
    return;
  }
  channel_batch->previousBatchInError = 0;
  if (channel_batch->status == STATUS_DONE || (channel->kind==CURRENT && channel_batch->status == STATUS_SAMPLING))
  {
    float_t calc = channel_batch->sumSquaresSamples / (float_t)channel_batch->numSamples;
    float_t mean = channel_batch->sumSamples / (float_t)channel_batch->numSamples;
    float_t Mrms = sqrt(calc - mean * mean) * channel->calibratedRatio;
    float_t Preal = 0;
    float_t Papp = 0;
    if (channel->kind == CURRENT)
    {
      float_t Vrms = channels[0].global.lastVrms;
      float_t Irms = Mrms;
      Papp = Vrms * Irms;
      Preal = (
        channel_batch->sumUISamples / (float_t)channel_batch->numSamples
                - channels[0].global.lastVavg * channel_batch->sumSamples/(float_t)channel_batch->numSamples
              ) 
              * channel->calibratedRatio * channels[0].info.calibratedRatio;
      if (Preal > Papp) {
        Preal = 0;
        Papp = 0;
        Mrms = 0;
      } else {
        uint32_t tmr = micros();
        if (channel_index->lastEnergyIndexTime>tmr-10000000) {
          if (Preal > 0)
          {
            channel_index->activeEnergyIndex += (tmr - channel_index->lastEnergyIndexTime) * Preal / (3600.0*1000000.0);
          }
          else
          {
            channel_index->reactiveEnergyIndex += (tmr - channel_index->lastEnergyIndexTime) * Preal / (3600.0*1000000.0);
          }
        }
        channel_index->lastEnergyIndexTime = tmr;
      }
    }
    else
    {
      channel_global->lastVrms = Mrms;
      channel_global->lastVavg = channel_batch->sumSamples / (float_t)channel_batch->numSamples;
    }
    Serial.printf("Channel=%d\tState=%d\t%srms=%.3f\tUnit=%s\tPapp=%.3f\tPreal=%.3f\tEact=%.3f\tEreact=%.3f\tSamplesCount=%d\tMean=%f\tFirstCCountDeltaOfBatch=%d\tMeanCCountDelta=%f\tTime=%d\tBatchDeltaCCount=%d",
                  channel->channel,
                  channel_batch->status,
                  channel->kind == VOLTAGE ? "V" : "I", 
                  Mrms, 
                  channel->kind == VOLTAGE ? "V" : "A",
                  Papp, Preal,
                  channel_index->activeEnergyIndex,
                  channel_index->reactiveEnergyIndex,
                  channel_batch->numSamples,
                  mean,
                  channel_batch->firstCCountDeltaOfBatch,
                  channel_batch->sumCCountDeltaOfBatch / (channel_batch->numSamples - 1),
                  calculateBatchTimeDurationMs(channel_batch),
                  channel_batch->endOfBatchCCount - channel_batch->startOfBatchCCount);
    Serial.println();
    if (channel_global->authorized_additional_offset)
    {
      if (channel_global->dynamic_offset == 0x7fff)
      channel_global->dynamic_offset = mean;
      else
      {
        if (abs(mean) < channel_global->authorized_additional_offset)
        {
          channel_global->dynamic_offset += mean;
          channel_global->authorized_additional_offset >>= 1;
        }
      }
    }
  }
  channel_batch->firstCCountDeltaOfBatch = 0;
  channel_batch->sumCCountDeltaOfBatch = 0;
  channel_batch->status = STATUS_LOOKING_FOR_ZERO;
}