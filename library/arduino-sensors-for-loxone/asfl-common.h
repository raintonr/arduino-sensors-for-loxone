#ifndef ASFL_COMMON_H
#define ASFL_COMMON_H

#include <Arduino.h>

void error_flash(uint16_t ms);
void error_flash(uint16_t flash, uint16_t wait);

// Moving average instances for readings to output
class MovingAverageCalculator {
 public:
  MovingAverageCalculator(int readings, int float_dps);
  MovingAverageCalculator(int readings);
  float sample(float input);
  int sample(int input);
  uint16_t sample(uint16_t input);

 private:
  int MovingAverageCalculator::new_reading(int input);
  long total;
  int output;
  int readings;
  int float_factor;
  boolean init;
};

class LogarithmicRegressionCalculator {
 public:
  LogarithmicRegressionCalculator(float add, float mult);
  float calc(float x);

 private:
  float add, mult;
};

#endif