#pragma once

#include <stdint.h>

// Frees an I2C bus left stuck by a target, before a controller that has no timeout touches it. The nRF52
// TWIM driver waits forever for a transfer to finish, so a target holding SDA low (reset or browned out
// mid-byte) or a powered-down target clamping the lines would hang the node for good.
//
// Bus supplies the pins, driven open-drain:
//   bool sda(), bool scl()                    line levels
//   void releaseSda(), void releaseScl()      let the line float high
//   void pullSdaLow(), void pullSclLow()
//   void halfPeriod()                         wait half a clock period

// Half periods to wait for a target stretching SCL before treating it as stuck: 1 ms at 100kHz.
#define I2C_RECOVERY_STRETCH_HALF_PERIODS 200

// A target in the middle of a byte releases SDA within 9 clocks (8 data bits and the ack).
#define I2C_RECOVERY_CLOCKS 9

template <typename Bus>
static bool i2cWaitSclHigh(Bus& bus) {
  for (uint16_t i = 0; i < I2C_RECOVERY_STRETCH_HALF_PERIODS; i++) {
    if (bus.scl()) return true;
    bus.halfPeriod();
  }
  return bus.scl();
}

// Returns true if the bus is left idle, both lines high. False if SCL is held low, which no controller
// can clear, or SDA is still held low after the recovery clocks.
template <typename Bus>
bool i2cRecoverBus(Bus& bus) {
  bus.releaseSda();
  bus.releaseScl();
  bus.halfPeriod();
  if (!i2cWaitSclHigh(bus)) return false;
  if (bus.sda()) return true;

  for (uint8_t i = 0; i < I2C_RECOVERY_CLOCKS && !bus.sda(); i++) {
    bus.pullSclLow();
    bus.halfPeriod();
    bus.releaseScl();
    bus.halfPeriod();
    if (!i2cWaitSclHigh(bus)) return false;
  }
  if (!bus.sda()) return false;

  // A STOP (SDA rising while SCL is high) puts every target back to waiting for a START.
  bus.pullSclLow();
  bus.halfPeriod();
  bus.pullSdaLow();
  bus.halfPeriod();
  bus.releaseScl();
  bus.halfPeriod();
  if (!i2cWaitSclHigh(bus)) return false;
  bus.releaseSda();
  bus.halfPeriod();
  return bus.sda() && bus.scl();
}
