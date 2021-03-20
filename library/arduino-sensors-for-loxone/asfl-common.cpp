#include <asfl-common.h>

// Flash our LED to indicate an error

void error_flash(uint16_t ms) {
  unsigned long stopFlash = millis() + ms;
  while (stopFlash > millis()) {
    digitalWrite(LED_BUILTIN, (millis() % 40 < 10) ? 1 : 0);
  }
  digitalWrite(LED_BUILTIN, 0);
}

void error_flash(uint16_t flash, uint16_t wait) {
  error_flash(flash);
  delay(wait);
}

// Moving average calculator...
MovingAverageCalculator::MovingAverageCalculator(int readings, int float_dps) {
  this->readings = readings;
  this->float_factor = pow(10, float_dps);
  this->init = false;
}
MovingAverageCalculator::MovingAverageCalculator(int readings) {
  this->readings = readings;
  this->float_factor = 0;
  this->init = false;
}
float MovingAverageCalculator::sample(float input) {
  if (this->float_factor > 1) {
    // Convert to integer, but multiply up to preserve required DPs
    input *= this->float_factor;
    // Round to nearest when we convert to int
    input += 0.5;
  }
  // Cast and average in the same way as integer
  float output = this->new_reading((int)input);
  // And scale output back
  return output / this->float_factor;
}
int MovingAverageCalculator::sample(int input) {
  return this->new_reading(input);
}
uint16_t MovingAverageCalculator::sample(uint16_t input) {
  return this->new_reading(input);
}

int MovingAverageCalculator::new_reading(int input) {
  if (!init) {
    init = true;
    this->total = input;
    this->total *= this->readings;
    this->output = input;
  } else {
    // Subtract one reading from total to give (MA_READINGS - 1) values
    // totalled up.
    this->total -= this->output;
    // Add the value passed in
    this->total += input;
    // And divide the total by readings to get the current average
    this->output = this->total / this->readings;
  }
  return this->output;
}

// Logarithmic regression calculator...
LogarithmicRegressionCalculator::LogarithmicRegressionCalculator(float add,
                                                                 float mult) {
  this->add = add;
  this->mult = mult;
}

float LogarithmicRegressionCalculator::calc(float x) {
  float y = 0;
  if (x > 0) {
    y = log(x);
    y *= mult;
    y += add;
    if (y < 0) y = 0;
  }
  return y;
}