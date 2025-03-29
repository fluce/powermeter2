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
  channel->channel = channel_number;
  channel->kind = kind;
  channel->value = 0;
  channel->adc_precalculated_input = reverse(input_array[channel->channel].val);
  channel->numSamples = 0;
  channel->sumSamples = 0;
  channel->sumSquaresSamples = 0;
  channel->calibratedRatio = calibratedRatio;
  channel->dynamic_offset = 0x7fff;
  channel->authorized_additional_offset = 1024;
  channel->deltaToVoltage = delta_to_voltage;
}

#define XTHAL_GET_CCOUNT() ({ int __ccount; \
    __asm__ __volatile__("rsr.ccount %0" : "=a"(__ccount)); \
    __ccount; })

#define CALIBRATED_TIMER_FREQ 79200
uint32_t calculateBatchTimeDurationMs(channel_t *channel)
{
  return (channel->endOfBatchTime - channel->startOfBatchTime) / 1000;
}

void process_complete(channel_t *channel)
{
  static uint32_t last_ccount = 0;
  uint32_t ccount = XTHAL_GET_CCOUNT();
  uint32_t rel_time = channel->lastSampleCCount == 0 ? 0 : ccount - channel->lastSampleCCount;
  channel->lastSampleCCount = ccount;

  ringbuffer[ringbuffer_head].rel_time = last_ccount == 0 ? 0 : ccount - last_ccount;
  last_ccount = ccount;
  ringbuffer[ringbuffer_head].channel = channel->channel;
  ringbuffer[ringbuffer_head].value = channel->value;

  if (channel->firstCCountDeltaOfBatch == 0)
  {
    channel->firstCCountDeltaOfBatch = rel_time;
    channel->sumCCountDeltaOfBatch = 0;
  }
  else
  {
    channel->sumCCountDeltaOfBatch += rel_time;
  }
  int8_t sign = channel->value >= 0 ? 1 : -1;
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
    channel->sumSamples += channel->value;
    channel->sumSquaresSamples += channel->value * channel->value;
    if (channel->kind == CURRENT)
    {
      int16_t current = channel->value;
      int16_t voltage = ringbuffer[(ringbuffer_head + channel->deltaToVoltage) & BUFFER_SIZE_MASK].value;
      channel->sumUISamples += current * voltage;      
    }
  }
  ringbuffer_head = (ringbuffer_head + 1) & BUFFER_SIZE_MASK;
}

void process_simple(channel_t *channel)
{
  static uint32_t last_ccount = 0;
  uint32_t ccount = XTHAL_GET_CCOUNT();
  uint32_t rel_time = channel->lastSampleCCount == 0 ? 0 : ccount - channel->lastSampleCCount;
  channel->lastSampleCCount = ccount;

  ringbuffer[ringbuffer_head].rel_time = last_ccount == 0 ? 0 : ccount - last_ccount;
  last_ccount = ccount;
  ringbuffer[ringbuffer_head].channel = channel->channel;
  ringbuffer[ringbuffer_head].value = channel->value;

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
    channel->sumSamples += channel->value;
    channel->sumSquaresSamples += channel->value * channel->value;
    if (channel->kind == CURRENT)
    {
      int16_t current = channel->value;
      int16_t voltage = ringbuffer[(ringbuffer_head + channel->deltaToVoltage) & BUFFER_SIZE_MASK].value;
      channel->sumUISamples += current * voltage;      
    }
  }
  ringbuffer_head = (ringbuffer_head + 1) & BUFFER_SIZE_MASK;
}

void process(channel_t *channel)
{
  if (channel->kind == CURRENT)
  {
    process_simple(channel);
  }
  else
  {
    process_complete(channel);
  }
}

void processBatch(channel_t *channel)
{
  // Serial.printf("Batch done : processing %d samples (status: %d)\n", channel->numSamples, channel->status);
  if (channel->status == STATUS_DONE_WITH_ERROR)
  {
    if (!channel->previousBatchInError)
    {
      channel->previousBatchInError = 1;
      Serial.printf("Channel=%d\tTime=%d\tNoSignal\r\n", channel->channel, calculateBatchTimeDurationMs(channel));
      Serial.flush();
    }
    return;
  }
  channel->previousBatchInError = 0;
  if (channel->status == STATUS_DONE || (channel->kind==CURRENT && channel->status == STATUS_SAMPLING))
  {
    float_t calc = channel->sumSquaresSamples / (float_t)channel->numSamples;
    float_t mean = channel->sumSamples / (float_t)channel->numSamples;
    float_t Mrms = sqrt(calc - mean * mean) * channel->calibratedRatio;
    float_t Preal = 0;
    float_t Papp = 0;
    if (channel->kind == CURRENT)
    {
      float_t Vrms = channels[0].lastVrms;
      float_t Irms = Mrms;
      Papp = Vrms * Irms;
      Preal = (
                  channel->sumUISamples / (float_t)channel->numSamples
                - channels[0].lastVavg * channel->sumSamples/(float_t)channel->numSamples
              ) 
              * channel->calibratedRatio * channels[0].calibratedRatio;
      if (Preal > Papp) {
        Preal = 0;
        Papp = 0;
        Mrms = 0;
      } else {
        uint32_t tmr = micros();
        if (channel->lastEnergyIndexTime>tmr-10000000) {
          if (Preal > 0)
          {
            channel->activeEnergyIndex += (tmr - channel->lastEnergyIndexTime) * Preal / (3600.0*1000000.0);
          }
          else
          {
            channel->reactiveEnergyIndex += (tmr - channel->lastEnergyIndexTime) * Preal / (3600.0*1000000.0);
          }
        }
        channel->lastEnergyIndexTime = tmr;
      }
    }
    else
    {
      channel->lastVrms = Mrms;
      channel->lastVavg = channel->sumSamples / (float_t)channel->numSamples;
    }
    Serial.printf("Channel=%d\tState=%d\t%srms=%.3f\tUnit=%s\tPapp=%.3f\tPreal=%.3f\tEact=%.3f\tEreact=%.3f\tSamplesCount=%d\tMean=%f\tFirstCCountDeltaOfBatch=%d\tMeanCCountDelta=%f\tTime=%d\tBatchDeltaCCount=%d",
                  channel->channel,
                  channel->status,
                  channel->kind == VOLTAGE ? "V" : "I", 
                  Mrms, 
                  channel->kind == VOLTAGE ? "V" : "A",
                  Papp, Preal,
                  channel->activeEnergyIndex,
                  channel->reactiveEnergyIndex,
                  channel->numSamples,
                  mean,
                  channel->firstCCountDeltaOfBatch,
                  channel->sumCCountDeltaOfBatch / (channel->numSamples - 1),
                  calculateBatchTimeDurationMs(channel),
                  channel->endOfBatchCCount - channel->startOfBatchCCount);
    Serial.println();
    if (channel->authorized_additional_offset)
    {
      if (channel->dynamic_offset == 0x7fff)
        channel->dynamic_offset = mean;
      else
      {
        if (abs(mean) < channel->authorized_additional_offset)
        {
          channel->dynamic_offset += mean;
          channel->authorized_additional_offset >>= 1;
        }
      }
    }
  }
  channel->firstCCountDeltaOfBatch = 0;
  channel->sumCCountDeltaOfBatch = 0;
  channel->status = STATUS_LOOKING_FOR_ZERO;
  /*channel->numSamples=0;
  channel->sumSamples=0;
  channel->sumSquaresSamples=0;  */
}