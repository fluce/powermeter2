#include <Arduino.h>
#include "capture.h"
#include "channel.h"
#include "buffer.h"
#include "reference_voltage.h"

#define BUTTON_PIN 0
bool dumpBufferOnFull = false;

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
}

TaskHandle_t Task1;

void task1code(void *parameter) {
  while (true) {

  }
}

void loop() {
  int ringbuffer_head_copy = ringbuffer_head;
  for (int i = 0; i < CHANNELS; i++) {
    capture(&channels[(i + 1) % CHANNELS], &channels[i]);
  }
  if (channels[0].batch.status == STATUS_DONE || channels[0].batch.status == STATUS_DONE_WITH_ERROR) {
    for (int i = 0; i < CHANNELS; i++) {
      processBatch(&channels[i].info,&channels[i].batch,&channels[i].index,&channels[i].global);
      channels[i].batch.status = STATUS_LOOKING_FOR_ZERO;
    }
  }
  if (ringbuffer_head < ringbuffer_head_copy && dumpBufferOnFull) {
    dumpBuffer();
    dumpBufferOnFull = false;
  }
  if (digitalRead(BUTTON_PIN) == LOW) {
    dumpBufferOnFull = true;
  }
}