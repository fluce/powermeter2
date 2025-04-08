#include <Arduino.h>
#include "capture.h"
#include "channel.h"
#include "buffer.h"
#include "process_batch.h"
#include "reference_voltage.h"
#include "calibration.h"
#include "esphome/core/log.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"

static const char *TAG = "powermeter";

#define BUTTON_PIN 0
bool dumpBufferOnFull = false;

TaskHandle_t Task1;
SemaphoreHandle_t runBatchProcess = NULL;

void task1code(void *parameter) {
  uint32_t counter=0;
  while (true) {
    //try {
      for (int i = 0; i < CHANNELS; i++) {
        capture(&channels[(i + 1) % CHANNELS], &channels[i]);
      }
      if (channels[0].capturing_batch->status == STATUS_DONE || channels[0].capturing_batch->status == STATUS_DONE_WITH_ERROR) {
        //Serial.println("# Swapping batches");
        for (int i = 0; i < CHANNELS; i++) {
          /*Serial.printf("# Swapping batches %d\tstatus=%d\tprocessing_numSamples=%d\tcapturing_numSamples=%d\r\n", channels[i].info.channel, channels[i].processing_batch->status, channels[i].processing_batch->numSamples, channels[i].capturing_batch->numSamples);
          Serial.flush();*/
          if (i>0) {
            end_of_batch(channels[i].capturing_batch);
          }
          channel_batch_t* tmp = channels[i].capturing_batch;
          channels[i].capturing_batch = channels[i].processing_batch;
          prepare_new_batch(channels[i].capturing_batch);
          channels[i].processing_batch = tmp;
        }
        xSemaphoreGive(runBatchProcess);
        //Serial.println("# Batches swapped");
        vTaskDelay(1);
      }
    /*} catch (const std::exception &e) {
      ESP_LOGE(TAG, "Exception in task1code: %s", e.what());
    } catch (...) {
      ESP_LOGE(TAG, "Unknown exception in task1code");
    }*/
  }
}

#ifdef ESPHOME
void internal_setup() {
#else
void setup() {
#endif
  //Initialize serial and wait for port to open:
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(chipSelectPin, OUTPUT);
  auto clockDiv = spiFrequencyToClockDiv(4000000);
  spi = spiStartBus(FSPI, clockDiv, SPI_MODE0, MSBFIRST);
  spiAttachSCK(spi, SCK);
  spiAttachMISO(spi, MISO);
  spiAttachMOSI(spi, MOSI);
  //spiAttachSS(spi, 0, chipSelectPin);
  spiTransaction(spi, clockDiv, SPI_MODE0, MSBFIRST);

  setup(&channel_standard_voltage, 1, 1.0f, VOLTAGE);

  reference_voltage = 2.17f; //calibrateRefVoltage();
  
  setup(&channels[0], 0, get_calibration_ratio(reference_voltage,&channel_calibrations[0]), VOLTAGE); // Vref=2.1059V, 4096 steps, 886kOhm/2700Ohm
  setup(&channels[1], 2, get_calibration_ratio(reference_voltage,&channel_calibrations[1]), CURRENT, -1); // 10A/V
  setup(&channels[2], 3, get_calibration_ratio(reference_voltage,&channel_calibrations[2]), CURRENT, -2); // 20A/V
  setup(&channels[3], 4, get_calibration_ratio(reference_voltage,&channel_calibrations[3]), CURRENT, -3); // 10A/V
  setup(&channels[4], 5, get_calibration_ratio(reference_voltage,&channel_calibrations[4]), CURRENT, -4); // 10A/V
  setup(&channels[5], 6, get_calibration_ratio(reference_voltage,&channel_calibrations[5]), CURRENT, -5); // 10A/V
  setup(&channels[6], 7, get_calibration_ratio(reference_voltage,&channel_calibrations[6]), CURRENT, -6); // 20A/V

  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  runBatchProcess = xSemaphoreCreateBinary();

  //disableCore0WDT();

  xTaskCreate(
    task1code, /* Task function. */
    "Task1",   /* name of task. */
    10000,     /* Stack size of task */
    NULL,      /* parameter of the task */
    1,         /* priority of the task */
    &Task1    /* Task handle to keep track of created task */
    //0         /* pin task to core 0 */
  );
  ESP_LOGI(TAG, "Task1 created");
}


#ifdef ESPHOME
uint8_t internal_loop() {
#else
void loop() {
#endif
  int ringbuffer_head_copy = ringbuffer_head;

  uint8_t new_result_mask = 0;

  if (xSemaphoreTake(runBatchProcess, 0) == pdTRUE) {
    ESP_LOGI(TAG, "Handling batch");

    for (int i = 0; i < CHANNELS; i++) {
      /*Serial.printf("# Processing channel %d\tstatus=%d\tprocessing_numSamples=%d\tcapturing_numSamples=%d\r\n", channels[i].info.channel, channels[i].processing_batch->status, channels[i].processing_batch->numSamples, channels[i].capturing_batch->numSamples);
      Serial.flush();*/
      uint8_t res=process_batch(&channels[i].info,channels[i].processing_batch,&channels[i].index,&channels[i].global, &channels[i].measure);
      if (res != MEASURE_STATE_UNKNOWN)
        new_result_mask |= (1 << i);
      channels[i].processing_batch->status = STATUS_LOOKING_FOR_ZERO;
    }
    //Serial.println("# Processing done");     
  }

  if (ringbuffer_head < ringbuffer_head_copy && dumpBufferOnFull) {
    dump_buffer();
    dumpBufferOnFull = false;
  }
  if (digitalRead(BUTTON_PIN) == LOW) {
    dumpBufferOnFull = true;
  }
  #ifdef ESPHOME
  return new_result_mask;
  #endif
}

