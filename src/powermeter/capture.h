#ifndef CAPTURE_H
#define CAPTURE_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <soc/spi_struct.h>
#include "channel.h"

struct spi_struct_t {
    volatile spi_dev_t * dev;
#if !CONFIG_DISABLE_HAL_LOCKS
    xSemaphoreHandle lock;
#endif
    uint8_t num;
};

#define ADC_RESOLUTION 12
#define ADC_MAX_VALUE (1<<ADC_RESOLUTION)
#define ADC_APPROX_ZERO_VALUE (ADC_MAX_VALUE>>1)

extern uint8_t chipSelectPin;
extern spi_t* spi;

uint16_t capture(channel_t* channel, channel_t* previous_channel);

#endif
