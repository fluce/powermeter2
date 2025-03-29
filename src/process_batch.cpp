#include "process_batch.h"
#include "channel.h"

void prepare_new_batch(channel_batch_t* batch)
{
  uint8_t batchNumber = batch->batchNumber;
  uint32_t endOfBatchTime = batch->endOfBatchTime;
  memset(batch, 0, sizeof(channel_batch_t));
  batch->batchNumber = batchNumber;
  batch->startOfBatchTime = endOfBatchTime;
  batch->status = STATUS_LOOKING_FOR_ZERO;
}

#define CALIBRATED_TIMER_FREQ 79200
uint32_t calculateBatchTimeDurationMs(channel_batch_t *channel_batch)
{
  return (channel_batch->endOfBatchTime - channel_batch->startOfBatchTime) / 1000;
}

void process_batch(channel_info_t *channel, channel_batch_t *channel_batch, channel_index_t *channel_index, channel_global_t *channel_global)
{
  // Serial.printf("Batch done : processing %d samples (status: %d)\n", channel->numSamples, channel->status);
  if (channel_batch->status == STATUS_DONE_WITH_ERROR)
  {
    if (!channel_global->previousBatchInError)
    {
      channel_global->previousBatchInError = 1;
      Serial.printf("Channel=%d\tTime=%d\tNoSignal\r\n", channel->channel, calculateBatchTimeDurationMs(channel_batch));
      Serial.flush();
    }
    return;
  }
  channel_global->previousBatchInError = 0;
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
    Serial.printf("Channel=%d\tState=%d\t%srms=%.3f\tUnit=%s\tPapp=%.3f\tPreal=%.3f\tEact=%.3f\tEreact=%.3f\tSamplesCount=%d\tMean=%f\tTime=%d\tBatchDeltaCCount=%d",
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