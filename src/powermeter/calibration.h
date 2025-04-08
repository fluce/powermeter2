#pragma once
#include "types.h"

extern channel_calibration_t channel_calibrations[CHANNELS];
  
extern float_t get_calibration_ratio(float_t reference_voltage, channel_calibration_t *calibration);
