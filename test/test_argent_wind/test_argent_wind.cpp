#include <gtest/gtest.h>
#include <string.h>
#include "helpers/sensors/ArgentWind.h"

// A snapshot as the board serves it: 3.45 m/s mean, 6.78 m/s gust, heading east spread 0x0B61
// (16 degrees), 12.34 mm rain.
static void makeMap(uint8_t map[ARGENT_WIND_MAP_SIZE], uint8_t status) {
  const uint8_t m[ARGENT_WIND_MAP_SIZE] = {
    ARGENT_WIND_DEVICE_ID, ARGENT_WIND_MAP_VERSION, status, 0x2A,
    0x59, 0x01,             // 345
    0xA6, 0x02,             // 678
    0x00, 0x40,             // 0x4000, east
    0x61, 0x0B,             // 0x0B61, 16 degrees
    0xD2, 0x04,             // 1234
    0x10, 0x0E, 0x00, 0x00, // uptime 3600
  };
  memcpy(map, m, sizeof(m));
}

TEST(ArgentWindReading, DecodesLittleEndianFields) {
  uint8_t map[ARGENT_WIND_MAP_SIZE];
  makeMap(map, 0x07);
  ArgentWindReading r;

  ASSERT_TRUE(r.decode(map));

  EXPECT_EQ(345, r.wind_avg);
  EXPECT_EQ(678, r.wind_gust);
  EXPECT_EQ(0x4000, r.wind_direction);
  EXPECT_EQ(0x0B61, r.wind_direction_stddev);
  EXPECT_EQ(1234, r.rain_last_hour);
  EXPECT_TRUE(r.hasWind());
  EXPECT_TRUE(r.hasDirection());
  EXPECT_TRUE(r.hasRain());
}

TEST(ArgentWindReading, RejectsOtherDevices) {
  uint8_t map[ARGENT_WIND_MAP_SIZE];
  ArgentWindReading r;

  makeMap(map, 0x07);
  map[0] = 0x00;
  EXPECT_FALSE(r.decode(map));

  makeMap(map, 0x07);
  map[1] = ARGENT_WIND_MAP_VERSION + 1;
  EXPECT_FALSE(r.decode(map));
}

TEST(ArgentWindReading, StatusGatesEachValue) {
  uint8_t map[ARGENT_WIND_MAP_SIZE];
  ArgentWindReading r;

  makeMap(map, 0x00);
  ASSERT_TRUE(r.decode(map));
  EXPECT_FALSE(r.hasWind());
  EXPECT_FALSE(r.hasDirection());
  EXPECT_FALSE(r.hasRain());

  makeMap(map, ARGENT_WIND_STATUS_DIRECTION_VALID);
  ASSERT_TRUE(r.decode(map));
  EXPECT_FALSE(r.hasWind());
  EXPECT_TRUE(r.hasDirection());
  EXPECT_FALSE(r.hasRain());
}

TEST(ArgentWindReading, StddevSkippedOnlyWhenZeroWithoutDirection) {
  ArgentWindReading r;
  r.status = ARGENT_WIND_STATUS_DIRECTION_VALID;
  r.wind_direction_stddev = 0;      EXPECT_TRUE(r.hasDirectionStddev());   // steady wind
  r.status = 0;
  r.wind_direction_stddev = 0x0B61; EXPECT_TRUE(r.hasDirectionStddev());   // calm, but headings seen
  r.wind_direction_stddev = 0;      EXPECT_FALSE(r.hasDirectionStddev());  // no valid headings
}

TEST(ArgentWindReading, DirectionInDegrees) {
  ArgentWindReading r;
  r.wind_direction = 0x0000; EXPECT_EQ(0, r.directionDegrees());
  r.wind_direction = 0x4000; EXPECT_EQ(90, r.directionDegrees());
  r.wind_direction = 0x8000; EXPECT_EQ(180, r.directionDegrees());
  r.wind_direction = 0xC000; EXPECT_EQ(270, r.directionDegrees());
  r.wind_direction = 0xFFFF; EXPECT_EQ(0, r.directionDegrees());   // just west of north rounds to 0, not 360
  r.wind_direction = 0x00B6; EXPECT_EQ(1, r.directionDegrees());   // 182/65536 turn = 0.9998 deg
}

TEST(ArgentWindReading, DirectionStddevInTenthsOfDegrees) {
  ArgentWindReading r;
  r.wind_direction_stddev = 0x0000; EXPECT_EQ(0, r.directionStddevTenthsDegrees());
  r.wind_direction_stddev = 0x0B61; EXPECT_EQ(160, r.directionStddevTenthsDegrees());   // 15.996 deg
  r.wind_direction_stddev = 0x49E7; EXPECT_EQ(1039, r.directionStddevTenthsDegrees());  // the ceiling, 103.93 deg
  r.wind_direction_stddev = 0xFFFF; EXPECT_EQ(3600, r.directionStddevTenthsDegrees());
}

TEST(ArgentWindReading, RainInTenthsOfMm) {
  ArgentWindReading r;
  r.rain_last_hour = 0;     EXPECT_EQ(0, r.rainTenthsMm());
  r.rain_last_hour = 4;     EXPECT_EQ(0, r.rainTenthsMm());
  r.rain_last_hour = 5;     EXPECT_EQ(1, r.rainTenthsMm());
  r.rain_last_hour = 1234;  EXPECT_EQ(123, r.rainTenthsMm());
  r.rain_last_hour = 65535; EXPECT_EQ(6554, r.rainTenthsMm());
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
