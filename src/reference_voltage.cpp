#include "reference_voltage.h"
#include "capture.h"

float_t calibrateRefVoltage()
{
    uint32_t std_adc_level_sum = 0;
    for (int i = 0; i < 16; i++)
    {
        uint16_t std_adc_level = capture(&channel_standard_voltage, NULL);
        usleep(20000);
        Serial.printf("std adc level=%d\r\n", std_adc_level);
        std_adc_level_sum += std_adc_level;
    }
    std_adc_level_sum >>= 4;
    float_t reference_voltage = 2.0f * 4096.0f / (std_adc_level_sum);
    Serial.printf("std adc level=%d, ref voltage=%f\r\n", std_adc_level_sum, reference_voltage);
    return reference_voltage;
}

float_t reference_voltage;
