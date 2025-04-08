#include "esphome/core/log.h"

static const char *TAG = "powermeter";

#include "process_batch.h"
#include "channel.h"
#include "buffer.h"

void prepare_new_batch(channel_batch_t* batch)
{
  uint8_t batchNumber = batch->batchNumber;
  uint32_t endOfBatchTime = batch->endOfBatchTime;
  memset(batch, 0, sizeof(channel_batch_t));
  batch->batchCounter+=2;
  batch->batchNumber = batchNumber;
  batch->startOfBatchTime = endOfBatchTime;
  batch->status = STATUS_LOOKING_FOR_ZERO;
}

#define CALIBRATED_TIMER_FREQ 79200
uint32_t calculateBatchTimeDurationMs(channel_batch_t *channel_batch)
{
  return (channel_batch->endOfBatchTime - channel_batch->startOfBatchTime) / 1000;
}

uint8_t process_batch(channel_info_t *channel, channel_batch_t *channel_batch, channel_index_t *channel_index, channel_global_t *channel_global, channel_measure_t* channel_measure)
{
  uint32_t tmr = micros();
  uint8_t result = MEASURE_STATE_UNKNOWN;
  // Serial.printf("Batch done : processing %d samples (status: %d)\n", channel->numSamples, channel->status);
  if (channel_batch->status == STATUS_DONE_WITH_ERROR)
  {
    if (!channel_global->previousBatchInError)
    {
      channel_global->previousBatchInError = 1;
      Serial.printf("Channel=%d\tTime=%d\tNoSignal\r\n", channel->channel, calculateBatchTimeDurationMs(channel_batch));
      Serial.flush();
      channel_measure->state = MEASURE_STATE_ERROR;
    }
    return MEASURE_STATE_ERROR;
  }
  channel_global->previousBatchInError = 0;
  if (channel_batch->status == STATUS_DONE || (channel->kind==CURRENT && channel_batch->status == STATUS_SAMPLING))
  {
    float_t calc = channel_batch->sumSquaresSamples / (float_t)channel_batch->numSamples;
    float_t mean = channel_batch->sumSamples / (float_t)channel_batch->numSamples;
    float_t meanRaw = channel_batch->sumRawSamples / (float_t)channel_batch->numSamples;
    float_t Mrms = sqrt(calc - mean * mean) * channel->calibratedRatio;
    float_t Preal = 0;
    float_t Papp = 0;
    auto noSignal = meanRaw < 100;
    if (channel->kind == CURRENT)
    {
      float_t Vrms = channels[0].global.lastVrms;
      float_t Irms = Mrms;
      Papp = Vrms * Irms;
      Preal = (
        (int32_t)(channel_batch->sumUISamples / channel_batch->numSamples)
                - channels[0].global.lastVavg * channel_batch->sumSamples/(float_t)channel_batch->numSamples
              ) 
              * channel->calibratedRatio * channels[0].info.calibratedRatio;

      auto noVoltage = Vrms < 10;
      auto incoherentReading = Preal > Papp || Preal > 10000 || Papp > 10000 || (Papp > 3 * Preal && Irms<=0.05) || Papp>8*Preal;

      /*ESP_LOGI(TAG, "CHECK Channel=%d\tPreal=%.3f\tPapp=%.3f\tIrms=%.3f\tVrms=%.3f\tnoSignal=%d\tnoVoltage=%d\tincoherentReading=%d", channel->channel, Preal, Papp, Irms, Vrms, noSignal, noVoltage, incoherentReading);
      ESP_LOGI(TAG, "CHECK Channel=%d\tUIsum=%d\tUIn=%d\tUIavg=%d\tCorrection=%.3f\tcalI=%.3f\tcalV=%0.3f",
        channel->channel,
        (int32_t)channel_batch->sumUISamples, (int32_t)channel_batch->numSamples,
        (int32_t)channel_batch->sumUISamples / (int32_t)channel_batch->numSamples,
        channels[0].global.lastVavg * channel_batch->sumSamples/(float_t)channel_batch->numSamples,
        channel->calibratedRatio,
        channels[0].info.calibratedRatio
      );*/

      if (noSignal || noVoltage || incoherentReading)
      {
        channel_measure->Irms = 0;
        channel_measure->Vrms = 0;
        channel_measure->Papp = 0;
        channel_measure->Preal = 0;      
        channel_measure->power_factor = 1;
        channel_measure->phase = 0;
        channel_measure->timestamp = tmr;
        channel_measure->state = noSignal || noVoltage ? MEASURE_STATE_OPEN : MEASURE_STATE_IDLE;
      } 
      else
      {
        channel_measure->Irms = Mrms;
        channel_measure->Vrms = Vrms;
        channel_measure->Papp = Papp;
        channel_measure->Preal = Preal;      
        channel_measure->power_factor = Preal / Papp;
        channel_measure->phase = acos(channel_measure->power_factor)*180/M_PI;
        channel_measure->state = MEASURE_STATE_ACTIVE;
        channel_measure->timestamp = tmr;

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
      }
      channel_index->lastEnergyIndexTime = tmr;
      result=channel_measure->state;
    }
    else
    {
      if (noSignal)
      {
        channel_measure->state = MEASURE_STATE_OPEN;
        channel_measure->Vrms = 0;
        channel_global->lastVrms = 0;
        channel_global->lastVavg = 0;
      } 
      else 
      {
        channel_measure->state = MEASURE_STATE_ACTIVE;
        channel_measure->Vrms = Mrms;
        channel_global->lastVrms = Mrms;
        channel_global->lastVavg = channel_batch->sumSamples / (float_t)channel_batch->numSamples;
      }
      result=channel_measure->state;
    }
    if (abs(channel_global->dynamic_offset)>200)
    {
      channel_global->dynamic_offset = 0x7fff;
      channel_global->authorized_additional_offset = 1024;
    } 
    else 
    {
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
    ESP_LOGI(TAG, "Channel=%d\tState=%d\tStateM=%d\t%srms=%.3f\tUnit=%s\tPapp=%.3f\tPreal=%.3f\tEact=%.3f\tEreact=%.3f\tSamplesCount=%d\tMean=%f\tOffset=%d\tTime=%d\tBatchDeltaCCount=%d",
                  channel->channel,
                  channel_batch->status,
                  channel_measure->state,
                  channel->kind == VOLTAGE ? "V" : "I", 
                  Mrms, 
                  channel->kind == VOLTAGE ? "V" : "A",
                  Papp, Preal,
                  channel_index->activeEnergyIndex,
                  channel_index->reactiveEnergyIndex,
                  channel_batch->numSamples,
                  mean,
                  channel_global->dynamic_offset,
                  calculateBatchTimeDurationMs(channel_batch),
                  channel_batch->endOfBatchCCount - channel_batch->startOfBatchCCount);
  }
  channel_batch->firstCCountDeltaOfBatch = 0;
  channel_batch->sumCCountDeltaOfBatch = 0;
  channel_batch->sumUISamples = 0;
  channel_batch->sumSamples = 0;
  channel_batch->sumRawSamples = 0;
  channel_batch->sumSquaresSamples = 0;
  channel_batch->numSamples = 0;  
  channel_batch->status = STATUS_LOOKING_FOR_ZERO;
 
  if (channel->channel==2 && ++channel->batch_counter==10) {
  } 
    /*for (int i = 1; i < BUFFER_SIZE; i++) {
      if (ringbuffer[i].channel == 2) {
        ESP_LOGI(TAG, "%d %d %d %d %d %d", i, ringbuffer[i].rel_time, ringbuffer[i].channel, ringbuffer[i].value, ringbuffer[i-1].channel, ringbuffer[i-1].value);
      }
    }
  }*/
  if (log_item_index > 0) {
    ESP_LOGI(TAG, "Log items: %d", log_item_index);
    for (int i = 0; i < log_item_index; i++) {
      ESP_LOGI(TAG, "%d %d %d %d %d %d", 
          log_items[i].index, log_items[i].voltage, log_items[i].current, 
          (int32_t)log_items[i].sumUI, 
          (int32_t)(log_items[i].sumUI/log_items[i].index),
          (int32_t)log_items[i].avgUI);
    }
    log_item_index = 0;
  }
  return result;
}

