#include <Arduino.h>
#include "capture.h"
#include "channel.h"
#include "buffer.h"
#include "process_batch.h"
#include "reference_voltage.h"

#define BUTTON_PIN 0
bool dumpBufferOnFull = false;

TaskHandle_t Task1;
SemaphoreHandle_t runBatchProcess = NULL;

void task1code(void *parameter) {
  while (true) {
    for (int i = 0; i < CHANNELS; i++) {
      capture(&channels[(i + 1) % CHANNELS], &channels[i]);
    }
    if (channels[0].capturing_batch->status == STATUS_DONE || channels[0].capturing_batch->status == STATUS_DONE_WITH_ERROR) {
      //Serial.println("# Swapping batches");
      for (int i = 0; i < CHANNELS; i++) {
        /*Serial.printf("# Swapping batches %d\tstatus=%d\tprocessing_numSamples=%d\tcapturing_numSamples=%d\r\n", channels[i].info.channel, channels[i].processing_batch->status, channels[i].processing_batch->numSamples, channels[i].capturing_batch->numSamples);
        Serial.flush();*/
        channel_batch_t* tmp = channels[i].capturing_batch;
        channels[i].capturing_batch = channels[i].processing_batch;
        prepare_new_batch(channels[i].capturing_batch);
        channels[i].processing_batch = tmp;
      }
      xSemaphoreGive(runBatchProcess);
      //Serial.println("# Batches swapped");
    }
  }
}

void setup() {
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

  usleep(500000);
  reference_voltage = 2.17f; //calibrateRefVoltage();
  float_t base_adc_ratio = reference_voltage / 4096.0f;

  setup(&channels[0], 0, base_adc_ratio * (855000.0f / 2427.0f), VOLTAGE); // Vref=2.1059V, 4096 steps, 886kOhm/2700Ohm
  setup(&channels[1], 2, base_adc_ratio * 10.0f, CURRENT, -1); // 10A/V
  setup(&channels[2], 3, base_adc_ratio * 20.0f, CURRENT, -2); // 20A/V
  setup(&channels[3], 4, base_adc_ratio * 10.0f, CURRENT, -3); // 10A/V
  setup(&channels[4], 5, base_adc_ratio * 10.0f, CURRENT, -4); // 10A/V
  setup(&channels[5], 6, base_adc_ratio * 10.0f, CURRENT, -5); // 10A/V
  setup(&channels[6], 7, base_adc_ratio * 20.0f, CURRENT, -6); // 20A/V

  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  runBatchProcess = xSemaphoreCreateBinary();

  xTaskCreatePinnedToCore(
    task1code, /* Task function. */
    "Task1",   /* name of task. */
    10000,     /* Stack size of task */
    NULL,      /* parameter of the task */
    1,         /* priority of the task */
    &Task1,    /* Task handle to keep track of created task */
    0);        /* pin task to core 0 */
}


void loop() {
  int ringbuffer_head_copy = ringbuffer_head;

  if (xSemaphoreTake(runBatchProcess, 10) == pdTRUE) {

    for (int i = 0; i < CHANNELS; i++) {
      /*Serial.printf("# Processing channel %d\tstatus=%d\tprocessing_numSamples=%d\tcapturing_numSamples=%d\r\n", channels[i].info.channel, channels[i].processing_batch->status, channels[i].processing_batch->numSamples, channels[i].capturing_batch->numSamples);
      Serial.flush();*/
      process_batch(&channels[i].info,channels[i].processing_batch,&channels[i].index,&channels[i].global);
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
}

