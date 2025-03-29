#include "capture.h"
#include "channel.h"

uint8_t chipSelectPin = 10;
spi_t* spi;

uint16_t capture(channel_t* channel, channel_t* previous_channel) {
  digitalWrite(chipSelectPin, LOW);

  spi->dev->ms_dlen.ms_data_bitlen = 23;

  spi->dev->data_buf[0]=channel->adc_precalculated_input;
  spi->dev->cmd.update = 1;
  int i1=0,i2=0;
  while(spi->dev->cmd.update) i1++;
  spi->dev->cmd.usr = 1;
  if (previous_channel) process(previous_channel);

  while(spi->dev->cmd.usr) i2++;

  uint32_t val32=spi->dev->data_buf[0];
  uint8_t* val=(uint8_t*)(&val32);
  uint16_t value=(val[1]&0x0f)<<8 | val[2];
  digitalWrite(chipSelectPin, HIGH);
  channel->value=value-ADC_APPROX_ZERO_VALUE-(channel->dynamic_offset!=0x7fff?channel->dynamic_offset:0);
  //Serial.printf("Channel=%d Value=%d\n", channel->channel, value);

  if (!previous_channel) process(channel);

  return value;
}