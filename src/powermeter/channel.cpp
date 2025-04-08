#include "channel.h"
#include "buffer.h"

typedef union input_u
{
  struct
  {
    uint32_t filler2 : 22;
    uint32_t batch : 3;
    uint32_t single_diff_flag : 1;
    uint32_t start : 1;
    uint32_t filler : 5;
  } input;
  uint32_t val;
} input;

input_u input_array[8] = {
    {.input = {.batch = 0, .single_diff_flag = 1, .start = 1}},
    {.input = {.batch = 1, .single_diff_flag = 1, .start = 1}},
    {.input = {.batch = 2, .single_diff_flag = 1, .start = 1}},
    {.input = {.batch = 3, .single_diff_flag = 1, .start = 1}},
    {.input = {.batch = 4, .single_diff_flag = 1, .start = 1}},
    {.input = {.batch = 5, .single_diff_flag = 1, .start = 1}},
    {.input = {.batch = 6, .single_diff_flag = 1, .start = 1}},
    {.input = {.batch = 7, .single_diff_flag = 1, .start = 1}},
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
  channel->info.adc_precalculated_input = reverse(input_array[channel->info.channel].val);
  channel->info.calibratedRatio = calibratedRatio;
  channel->info.deltaToVoltage = delta_to_voltage;
  channel->global.dynamic_offset = 0x7fff;
  channel->global.authorized_additional_offset = 1024;
  channel->sample.value = 0;
  channel->capturing_batch = &channel->batches[0];
  channel->capturing_batch->batchNumber = 1;
  channel->processing_batch = &channel->batches[1];
  channel->processing_batch->batchNumber = 2;
}

#define XTHAL_GET_CCOUNT() ({ int __ccount; \
    __asm__ __volatile__("rsr.ccount %0" : "=a"(__ccount)); \
    __ccount; })

void process_sample_complete(channel_info_t *info, sample_t *sample, channel_batch_t *batch)
{
  static uint32_t last_ccount = 0;
  uint32_t ccount = XTHAL_GET_CCOUNT();
  uint32_t rel_time = batch->lastSampleCCount == 0 ? 0 : ccount - batch->lastSampleCCount;
  batch->lastSampleCCount = ccount;

  ringbuffer[ringbuffer_head].rel_time = last_ccount == 0 ? 0 : ccount - last_ccount;
  last_ccount = ccount;
  ringbuffer[ringbuffer_head].channel = info->channel;
  ringbuffer[ringbuffer_head].value = sample->value;

  if (batch->firstCCountDeltaOfBatch == 0)
  {
    batch->firstCCountDeltaOfBatch = rel_time;
    batch->sumCCountDeltaOfBatch = 0;
  }
  else
  {
    batch->sumCCountDeltaOfBatch += rel_time;
  }
  int8_t sign = sample->value >= 0 ? 1 : -1;
  if (sign == sample->lastSign)
  {
    batch->sameSignNumSamples++;
  }
  else
  {
    sample->lastSign = sign;
    uint32_t tmr = micros();
    bool justStartedSampling = false;
    if (batch->status == STATUS_LOOKING_FOR_ZERO)
    {
      if (batch->sameSignNumSamples > 5) 
      {
        //Serial.printf("# %d Channel=%d Batch=%d Start sampling\r\n", tmr, info->channel, batch->batchNumber);
        //Serial.flush();
        batch->status = STATUS_SAMPLING;
        batch->startIndex = ringbuffer_head;
        batch->numSamples = 0;
        batch->sumSamples = 0;
        batch->sumSquaresSamples = 0;
        batch->sumUISamples = 0;
        batch->numHalfPeriod = 0;
        batch->startOfBatchTime = tmr;
        batch->startOfBatchCCount = ccount;
        justStartedSampling = true;
      } else {
        /*Serial.printf("# %d Channel=%d Batch=%d Waiting for starting to sample (%d)\r\n", 
          tmr, info->channel, batch->batchNumber, batch->sameSignNumSamples);
        Serial.flush();*/
      }
    }
    batch->numHalfPeriod++;
    uint32_t batchDuration = (tmr - batch->startOfBatchTime) / 1000;
/*    Serial.printf("%d Channel=%d status=%d ssNs=%d signChange %d %d %d\n", 
      tmr, info->channel, batch->status, 
      batch->sameSignNumSamples, 
      sample->lastSign, batch->numHalfPeriod, batchDuration);*/
    batch->sameSignNumSamples = 1;
    if (batch->numHalfPeriod == HALF_PERIOD_BY_BATCH)
    {
      // Serial.printf("Batch duration : %d\n", batchDuration);
      batch->endOfBatchTime = tmr;
      batch->endOfBatchCCount = ccount;
      if (batchDuration > 980 && batchDuration < 1010)
      {
        batch->status = STATUS_DONE;
      }
      else
      {
        /*Serial.printf("# %d Channel=%d Batch=%d\tstatus=%d (%d)\tnumHalfPeriod=%d\tduration=%d\tBatchDurationError\r\n", 
            tmr,
            info->channel, 
            batch->batchNumber,
            batch->status,
            justStartedSampling,
            batch->numHalfPeriod,
            batchDuration);
        Serial.flush();*/
        batch->status = STATUS_DONE_WITH_ERROR;
      }
    }
    else if (batchDuration > 1200)
    {
      /*Serial.printf("# %d Channel=%d Batch=%d\tstatus=%d (%d)\tnumHalfPeriod=%d\tduration=%d\tBatchDurationError\r\n", 
        tmr,
        info->channel,
        batch->batchNumber,
        batch->status,
        justStartedSampling,
        batch->numHalfPeriod,
        batchDuration);
      Serial.flush();*/
      batch->status = STATUS_DONE_WITH_ERROR;
    }
  }
  if (batch->status == STATUS_SAMPLING)
  {
    batch->numSamples++;
    batch->sumSamples += sample->value;
    batch->sumRawSamples += sample->raw_value;
    batch->sumSquaresSamples += sample->value * sample->value;
    if (info->kind == CURRENT)
    {
      int16_t current = sample->value;
      int16_t voltage = ringbuffer[(ringbuffer_head + info->deltaToVoltage) & BUFFER_SIZE_MASK].value;
      batch->sumUISamples += current * voltage;      
    }
  }
  ringbuffer_head = (ringbuffer_head + 1) & BUFFER_SIZE_MASK;
}

void process_sample_simple(channel_info_t *info, sample_t *sample, channel_batch_t *batch)
{
  static uint32_t last_ccount = 0;
  uint32_t ccount = XTHAL_GET_CCOUNT();
  uint32_t rel_time = batch->lastSampleCCount == 0 ? 0 : ccount - batch->lastSampleCCount;
  batch->lastSampleCCount = ccount;

  ringbuffer[ringbuffer_head].rel_time = last_ccount == 0 ? 0 : ccount - last_ccount;
  last_ccount = ccount;
  ringbuffer[ringbuffer_head].channel = info->channel;
  ringbuffer[ringbuffer_head].value = sample->value;

  if (batch->status == STATUS_LOOKING_FOR_ZERO)
  {
    uint32_t tmr = micros();
    batch->status = STATUS_SAMPLING;
    batch->startIndex = ringbuffer_head;
    batch->numSamples = 0;
    batch->sumSamples = 0;
    batch->sumSquaresSamples = 0;
    batch->sumUISamples = 0;
    batch->numHalfPeriod = 0;
    batch->startOfBatchTime = tmr;
    batch->startOfBatchCCount = ccount;
    if (info->channel==2) {
      ESP_LOGI(TAG, "Channel=%d Batch=%d Start sampling\r\n", info->channel, batch->batchNumber);
      log_item_index = 0;
    }
  }
  if (batch->status == STATUS_SAMPLING)
  {
    batch->numSamples++;
    batch->sumSamples += sample->value;
    batch->sumRawSamples += sample->raw_value;
    batch->sumSquaresSamples += sample->value * sample->value;
    if (info->kind == CURRENT)
    {
      int16_t current = sample->value;
      int16_t voltage = ringbuffer[(ringbuffer_head + info->deltaToVoltage) & BUFFER_SIZE_MASK].value;
      batch->sumUISamples += (int64_t)current * (int64_t)voltage;
      if (info->channel==2 && info->batch_counter==12 && log_item_index < 20) {
        log_items[log_item_index].index = batch->numSamples;
        log_items[log_item_index].voltage = voltage;
        log_items[log_item_index].current = current;
        int64_t sumUI = batch->sumUISamples;
        int64_t numSamples = batch->numSamples;
        log_items[log_item_index].sumUI = sumUI;
        log_items[log_item_index].avgUI = sumUI / numSamples;
        log_item_index++;
      }
    }
  }
  ringbuffer_head = (ringbuffer_head + 1) & BUFFER_SIZE_MASK;
}

void process_sample(channel_info_t *info, sample_t *sample, channel_batch_t *batch)
{
  if (info->kind == CURRENT)
  {
    process_sample_simple(info,sample,batch);
  }
  else
  {
    process_sample_complete(info,sample,batch);
  }
}

void end_of_batch(channel_batch_t* batch)
{
  batch->status = STATUS_DONE;
  batch->endOfBatchTime = micros();
  batch->endOfBatchCCount = XTHAL_GET_CCOUNT();
}