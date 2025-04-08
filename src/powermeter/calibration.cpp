#include "calibration.h"

channel_calibration_t channel_calibrations[CHANNELS] = {
    {1.0f, 1, (855000.0f / 2427.0f)}, // channel 0
    {10.0f, 1, 1.0f}, // channel 1
    {10.0f, 1, 1.0f}, // channel 2
    {10.0f, 1, 1.0f}, // channel 3
    {20.0f, 1, 1.0f}, // channel 4
    {20.0f, 1, 1.0f}, // channel 5
    {20.0f, 1, 1.0f}  // channel 6
};

float_t get_calibration_ratio(float_t reference_voltage, channel_calibration_t *calibration)
{
    return (reference_voltage / 4096.0f) * calibration->CT_amp_per_volt * calibration->CT_loops * calibration->ratio;
}