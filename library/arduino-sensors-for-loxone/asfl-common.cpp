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
  this->num_readings = readings;
  this->float_factor = pow(10, float_dps);
  this->init = false;
}
MovingAverageCalculator::MovingAverageCalculator(int readings) {
  this->num_readings = readings;
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

// We could just keep a total and subtract that divided by num_readings
// but this will eventually lead to rounding errors.
int MovingAverageCalculator::new_reading(int input) {
  if (!init) {
    init = true;
    this->total = input;
    this->total *= this->num_readings;
    // Allocate readings array and fill with this first value.
    this->readings = new int[this->num_readings];
    for (int lp = 0; lp < this->num_readings; lp++) {
      this->readings[lp] = input;
    }
    // Which is the next to be replaced?
    this->next_reading = 0;
  } else {
    // Update running total by deleting the oldest value and replacing with that
    // passed in.

    // Add the value passed in
    this->total += input;
    // Subtract the oldest reading from total.
    this->total -= this->readings[this->next_reading];
    // Replace that oldest reading with the one passed in
    this->readings[this->next_reading] = input;
    // And move on to the next slot
    if (++this->next_reading == this->num_readings) {
      this->next_reading = 0;
    }
  }
  return this->total / this->num_readings;
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