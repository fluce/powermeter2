#include <Arduino.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <soc/spi_struct.h>

uint8_t chipSelectPin=10;
uint32_t chipSelectMask=1<<chipSelectPin;

spi_t* spi;

struct spi_struct_t {
    volatile spi_dev_t * dev;
#if !CONFIG_DISABLE_HAL_LOCKS
    xSemaphoreHandle lock;
#endif
    uint8_t num;
};

typedef union input_u {
  struct {
    uint32_t filler2:22;
    uint32_t channel:3;
    uint32_t single_diff_flag:1;
    uint32_t start:1;
    uint32_t filler:5;
  } input;
  uint32_t val;
} input;

typedef union output_u {
  struct {
    uint32_t filler2:8;
    uint32_t value:12;
    uint32_t nul:1;
    uint32_t filler:11;
  } output;
  uint32_t val;
} output;


input_u input_array[8]={
  { .input={  .channel=0, .single_diff_flag=1, .start=1 }},
  { .input={  .channel=1, .single_diff_flag=1, .start=1 }},
  { .input={  .channel=2, .single_diff_flag=1, .start=1 }},
  { .input={  .channel=3, .single_diff_flag=1, .start=1 }},
  { .input={  .channel=4, .single_diff_flag=1, .start=1 }},
  { .input={  .channel=5, .single_diff_flag=1, .start=1 }},
  { .input={  .channel=6, .single_diff_flag=1, .start=1 }},
  { .input={  .channel=7, .single_diff_flag=1, .start=1 }},
};

uint32_t reverse(uint32_t val) {
  uint8_t* t= (uint8_t*)&val;
  return (((((t[0]<<8) | t[1])<<8) | t[2])<<8) | t[3];
}

__attribute__((always_inline)) static inline uint32_t timer_u32(void) {
  REG_WRITE(0x3f423038,1UL << 31);             // Write SYSTIMER_TIMER_UPDATE bit 
  while (!REG_GET_BIT(0x3f423038,1UL << 30));  // Check SYSTIMER_TIMER_VALUE_VALID
  return REG_READ(0x3f423040);                 // Read SYSTIMER_VALUE_LO_REG
}

typedef struct {
  uint16_t rel_time;
  uint8_t channel;
  int16_t value;
} buffer_item_t;

#define BUFFER_SIZE 16384

#define ADC_RESOLUTION 12
#define ADC_MAX_VALUE (1<<ADC_RESOLUTION)
#define ADC_APPROX_ZERO_VALUE (ADC_MAX_VALUE>>1)

#define STATUS_LOOKING_FOR_ZERO 0
#define STATUS_SAMPLING 1
#define STATUS_DONE 2
#define STATUS_DONE_WITH_ERROR 3

#define HALF_PERIOD_BY_BATCH 100

typedef enum {
  VOLTAGE,
  CURRENT
} channel_kind_t;

struct channel_t {
  uint8_t channel;
  channel_kind_t kind;
  float_t calibratedRatio;
  int16_t value;  
  uint32_t adc_precalculated_input;
  int16_t dynamic_offset;
  int16_t authorized_additional_offset;

  uint8_t status;
  uint32_t numSamples;
  int64_t sumSamples;
  uint64_t sumSquaresSamples;
  uint16_t sameSignNumSamples;
  int8_t lastSign;
  uint8_t numHalfPeriod;
  uint16_t startIndex;
  uint32_t firstCCountDeltaOfBatch;
  uint32_t sumCCountDeltaOfBatch;
  uint32_t startOfBatchTime;
  uint32_t endOfBatchTime;
  uint32_t startOfBatchCCount;
  uint32_t endOfBatchCCount;
  uint8_t previousBatchInError;
  uint32_t lastSampleCCount;
};

buffer_item_t ringbuffer[BUFFER_SIZE];
uint16_t ringbuffer_head=0;  

#define CALIBRATED_TIMER_FREQ 79200
uint32_t calculateBatchTimeDurationMs(channel_t *channel)
{
  return (channel->endOfBatchTime - channel->startOfBatchTime) / 1000;
}


void setup(channel_t* channel, uint8_t channel_number, float_t calibratedRatio, channel_kind_t kind=VOLTAGE) {
  channel->channel=channel_number;
  channel->kind=kind;
  channel->value=0;
  channel->adc_precalculated_input=reverse(input_array[channel->channel].val);
  channel->numSamples=0;
  channel->sumSamples=0;
  channel->sumSquaresSamples=0;
  channel->calibratedRatio=calibratedRatio;
  channel->dynamic_offset=0x7fff;
  channel->authorized_additional_offset=1024;
}

#define XTHAL_GET_CCOUNT()	({ int __ccount; \
  __asm__ __volatile__("rsr.ccount %0" : "=a"(__ccount)); \
  __ccount; })

void process(channel_t* channel) {
  static uint32_t last_ccount=0;
  uint32_t ccount=XTHAL_GET_CCOUNT();
  uint32_t rel_time=channel->lastSampleCCount==0?0:ccount-channel->lastSampleCCount;
  channel->lastSampleCCount=ccount;

  ringbuffer[ringbuffer_head].rel_time=last_ccount==0?0:ccount-last_ccount;
  last_ccount=ccount;
  ringbuffer[ringbuffer_head].channel=channel->channel;
  ringbuffer[ringbuffer_head].value=channel->value;

  if (channel->firstCCountDeltaOfBatch==0) {
    channel->firstCCountDeltaOfBatch=rel_time;
    channel->sumCCountDeltaOfBatch=0;
  } else {
    channel->sumCCountDeltaOfBatch+=rel_time;
  }
  int8_t sign=channel->value>=0?1:-1;
  if (sign==channel->lastSign) {
    channel->sameSignNumSamples++;
  } else {
    uint32_t tmr=micros();
    if (channel->sameSignNumSamples>5 && channel->status==STATUS_LOOKING_FOR_ZERO) {
      //Serial.printf("%d Channel=%d Start sampling\n", tmr, channel->channel);
      //Serial.flush();
      channel->status=STATUS_SAMPLING;
      channel->startIndex=ringbuffer_head;
      channel->numSamples=0;
      channel->sumSamples=0;
      channel->sumSquaresSamples=0;
      channel->numHalfPeriod=0;
      channel->startOfBatchTime=tmr;
      channel->startOfBatchCCount=ccount;
    }
    channel->sameSignNumSamples=1;
    channel->lastSign=sign;
    channel->numHalfPeriod++;
    uint32_t batchDuration=(tmr - channel->startOfBatchTime) / 1000;
    if (channel->numHalfPeriod==HALF_PERIOD_BY_BATCH) {
      //Serial.printf("Batch duration : %d\n", batchDuration);
      channel->endOfBatchTime=tmr;
      channel->endOfBatchCCount=ccount;
      if (batchDuration>980 && batchDuration<1010) {
        channel->status=STATUS_DONE;
      } else {
        channel->status=STATUS_DONE_WITH_ERROR;
      }
    } else if (batchDuration>1200) {
      channel->status=STATUS_DONE_WITH_ERROR;
    }
  }
  if (channel->status==STATUS_SAMPLING) {
    channel->numSamples++;
    channel->sumSamples+=channel->value;
    channel->sumSquaresSamples+=channel->value*channel->value;
  }
  ringbuffer_head = (ringbuffer_head + 1) % BUFFER_SIZE; 
}

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

void processBatch(channel_t *channel)
{
  //Serial.printf("Batch done : processing %d samples (status: %d)\n", channel->numSamples, channel->status);
  if (channel->status==STATUS_DONE_WITH_ERROR) {
    if (!channel->previousBatchInError) {
      channel->previousBatchInError=1;
      Serial.printf("Channel=%d\tTime=%d\tNoSignal\r\n", channel->channel, calculateBatchTimeDurationMs(channel));
      Serial.flush();
    }
    return;
  }
  channel->previousBatchInError=0;
  float_t calc=channel->sumSquaresSamples/(float_t)channel->numSamples;
  float_t mean=channel->sumSamples/(float_t)channel->numSamples;
  float_t Vrms=sqrt(calc-mean*mean)*channel->calibratedRatio;
  Serial.printf("Channel=%d\t%srms=%.3f\tUnit=%s\tSamplesCount=%d\tMean=%f\tFirstCCountDeltaOfBatch=%d\tMeanCCountDelta=%f\tTime=%d\tBatchDeltaCCount=%d", 
    channel->channel,
    channel->kind==VOLTAGE?"V":"I",Vrms,channel->kind==VOLTAGE?"V":"A",
    channel->numSamples, 
    mean, 
    channel->firstCCountDeltaOfBatch, 
    channel->sumCCountDeltaOfBatch/(channel->numSamples-1), 
    calculateBatchTimeDurationMs(channel), 
    channel->endOfBatchCCount-channel->startOfBatchCCount
  );
  Serial.println();
  if (channel->authorized_additional_offset) {
    if (channel->dynamic_offset==0x7fff)
      channel->dynamic_offset=mean;
    else {
      if (abs(mean)<channel->authorized_additional_offset) {
        channel->dynamic_offset+=mean;
        channel->authorized_additional_offset>>=1;
      }
    }
  }
  channel->firstCCountDeltaOfBatch=0;
  channel->sumCCountDeltaOfBatch=0;
  channel->status=STATUS_LOOKING_FOR_ZERO;
  /*channel->numSamples=0;
  channel->sumSamples=0;
  channel->sumSquaresSamples=0;  */
}

void dumpBuffer()
{
  Serial.println("Buffer full");
  for(int i=0;i<BUFFER_SIZE;i++) {
    Serial.printf("%d %d %d %d\r\n", i, ringbuffer[i].rel_time, ringbuffer[i].channel, ringbuffer[i].value);
  }
}

channel_t channel_standard_voltage;

float_t calibrateRefVoltage() {
  uint32_t std_adc_level_sum=0;
  for (int i=0;i<16;i++) {
    uint16_t std_adc_level=capture(&channel_standard_voltage,NULL);
    usleep(20000);
    Serial.printf("std adc level=%d\r\n", std_adc_level);
    std_adc_level_sum+=std_adc_level;
  }
  std_adc_level_sum>>=4;
  float_t reference_voltage=2.0f*4096.0f/(std_adc_level_sum);
  Serial.printf("std adc level=%d, ref voltage=%f\r\n", std_adc_level_sum, reference_voltage);
  return reference_voltage;
}

#define CHANNELS 7
channel_t channels[CHANNELS];

float_t reference_voltage;

#define BUTTON_PIN 0
bool dumpBufferOnFull = false;

void setup() {
  //Initialize serial and wait for port to open:
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(chipSelectPin, OUTPUT);
  auto clockDiv=spiFrequencyToClockDiv(4000000);
  spi=spiStartBus(FSPI,clockDiv,SPI_MODE0,MSBFIRST);
  spiAttachSCK(spi, SCK);
  spiAttachMISO(spi, MISO);
  spiAttachMOSI(spi, MOSI); 
  //spiAttachSS(spi, 0, chipSelectPin);  
  spiTransaction(spi,clockDiv,SPI_MODE0,MSBFIRST);

  setup(&channel_standard_voltage, 1, 1.0f, VOLTAGE);  
  
  usleep(500000);
  reference_voltage=2.17f;//calibrateRefVoltage();
  float_t base_adc_ratio=reference_voltage/4096.0f;

  setup(&channels[0], 0, base_adc_ratio*(855000.0f/2427.0f), VOLTAGE); // Vref=2.1059V, 4096 steps, 886kOhm/2700Ohm
  setup(&channels[1], 2, base_adc_ratio*10.0f, CURRENT); // 10A/V
  setup(&channels[2], 3, base_adc_ratio*20.0f, CURRENT); // 20A/V
  setup(&channels[3], 4, base_adc_ratio*10.0f, CURRENT); // 10A/V
  setup(&channels[4], 5, base_adc_ratio*10.0f, CURRENT); // 10A/V
  setup(&channels[5], 6, base_adc_ratio*10.0f, CURRENT); // 10A/V
  setup(&channels[6], 7, base_adc_ratio*20.0f, CURRENT); // 20A/V

  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  int ringbuffer_head_copy=ringbuffer_head;
  for (int i=0;i<CHANNELS;i++) {
    capture(&channels[(i+1)%CHANNELS],&channels[i]);
  }
  for (int i=0;i<CHANNELS;i++) {
    if (channels[i].status==STATUS_DONE || channels[i].status==STATUS_DONE_WITH_ERROR) { 
      processBatch(&channels[i]);
      channels[i].status=STATUS_LOOKING_FOR_ZERO;
    }
  }
  if (ringbuffer_head<ringbuffer_head_copy && dumpBufferOnFull) {
    dumpBuffer();
    dumpBufferOnFull = false;
  }
  if (digitalRead(BUTTON_PIN) == LOW) {
    dumpBufferOnFull = true;
    delay(500); // Debounce delay
  }
}