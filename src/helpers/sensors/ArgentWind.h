#pragma once

#include <stdint.h>

// I2C register map of the AgilentWindArduino interface board for the Argent Data wind and rain sensor
// assembly (its lib/WeatherCore/WeatherRegisters.h). Write register 0x00, then read the whole map for one
// consistent snapshot. Multi-byte values are little-endian.

#define ARGENT_WIND_ADDRESS 0x6C            // 0x6E with the board's address jumper fitted
#define ARGENT_WIND_ALT_ADDRESS 0x6E
#define ARGENT_WIND_DEVICE_ID 0x57          // 'W'
#define ARGENT_WIND_MAP_VERSION 1
#define ARGENT_WIND_MAP_SIZE 18

// Gap between the register pointer write and the read.
#define ARGENT_WIND_WRITE_TO_READ_US 100

#define ARGENT_WIND_STATUS_WIND_VALID 0x01          // a full wind window is in; until then wind reads 0
#define ARGENT_WIND_STATUS_DIRECTION_VALID 0x02
#define ARGENT_WIND_STATUS_RAIN_VALID 0x04          // a full rain hour is in; until then rain reads 0

struct ArgentWindReading {
  uint8_t  status = 0;
  uint16_t wind_avg = 0;        // 0.01 m/s
  uint16_t wind_gust = 0;       // 0.01 m/s
  uint16_t wind_direction = 0;  // 1/65536 turn clockwise from north
  uint16_t wind_direction_stddev = 0;  // Yamartino standard deviation of the headings, 1/65536 turn
  uint16_t rain_last_hour = 0;  // 0.01 mm

  // False if the map isn't from a board this code understands.
  bool decode(const uint8_t map[ARGENT_WIND_MAP_SIZE]) {
    if (map[0x00] != ARGENT_WIND_DEVICE_ID || map[0x01] != ARGENT_WIND_MAP_VERSION) return false;
    status = map[0x02];
    wind_avg = readU16(&map[0x04]);
    wind_gust = readU16(&map[0x06]);
    wind_direction = readU16(&map[0x08]);
    wind_direction_stddev = readU16(&map[0x0A]);
    rain_last_hour = readU16(&map[0x0C]);
    return true;
  }

  bool hasWind() const { return status & ARGENT_WIND_STATUS_WIND_VALID; }
  bool hasDirection() const { return status & ARGENT_WIND_STATUS_DIRECTION_VALID; }
  bool hasRain() const { return status & ARGENT_WIND_STATUS_RAIN_VALID; }

  // Rounded to the nearest degree, 0-359.
  uint16_t directionDegrees() const { return (uint16_t)((((uint32_t)wind_direction * 360 + 0x8000) >> 16) % 360); }
  // LPP_WIND_DIRECTION_STDDEV units, 0.1 degree, rounded.
  uint16_t directionStddevTenthsDegrees() const {
    return (uint16_t)(((uint32_t)wind_direction_stddev * 3600 + 0x8000) >> 16);
  }
  // LPP_RAIN units, 0.1 mm, rounded.
  uint16_t rainTenthsMm() const { return (uint16_t)((rain_last_hour + 5u) / 10u); }

private:
  static uint16_t readU16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
};
