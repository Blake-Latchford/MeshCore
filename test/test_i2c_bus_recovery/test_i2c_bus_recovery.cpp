#include <gtest/gtest.h>
#include "helpers/sensors/I2CBusRecovery.h"

// An open-drain bus with one target. Each line is high unless the controller or the target pulls it low.
// The target can hold SDA low until it sees a number of SCL rising edges, as one cut off mid-byte does, or
// clamp a line low for good, as an unpowered one does.
struct FakeBus {
  bool ctl_sda_low = false;
  bool ctl_scl_low = false;
  int  target_sda_clocks = 0;      // SCL rising edges until the target lets SDA go; 0 = not holding it
  bool target_sda_stuck = false;
  bool target_scl_stuck = false;
  int  scl_stretch_half_periods = 0;

  int  rising_edges = 0;
  int  stops = 0;
  int  half_periods = 0;

  bool targetHoldsSda() const { return target_sda_stuck || target_sda_clocks > 0; }
  bool targetHoldsScl() const { return target_scl_stuck || scl_stretch_half_periods > 0; }

  bool sda() const { return !ctl_sda_low && !targetHoldsSda(); }
  bool scl() const { return !ctl_scl_low && !targetHoldsScl(); }

  void releaseSda() {
    bool was_low = !sda();
    ctl_sda_low = false;
    if (was_low && sda() && scl()) stops++;
  }
  void releaseScl() {
    bool was_low = !scl();
    ctl_scl_low = false;
    if (was_low && scl()) risingEdge();
  }
  void pullSdaLow() { ctl_sda_low = true; }
  void pullSclLow() { ctl_scl_low = true; }

  void halfPeriod() {
    half_periods++;
    if (scl_stretch_half_periods > 0 && --scl_stretch_half_periods == 0 && scl()) risingEdge();
  }

  void risingEdge() {
    rising_edges++;
    if (target_sda_clocks > 0) target_sda_clocks--;
  }
};

TEST(I2CBusRecovery, IdleBusIsLeftAlone) {
  FakeBus bus;

  EXPECT_TRUE(i2cRecoverBus(bus));

  EXPECT_EQ(0, bus.rising_edges);
  EXPECT_FALSE(bus.ctl_sda_low);
  EXPECT_FALSE(bus.ctl_scl_low);
}

TEST(I2CBusRecovery, ClocksOutATargetHoldingSda) {
  FakeBus bus;
  bus.target_sda_clocks = 5;

  EXPECT_TRUE(i2cRecoverBus(bus));

  EXPECT_EQ(0, bus.target_sda_clocks);
  EXPECT_EQ(1, bus.stops);
  EXPECT_TRUE(bus.sda());
  EXPECT_TRUE(bus.scl());
}

TEST(I2CBusRecovery, NineClocksFreeAWholeByte) {
  FakeBus bus;
  bus.target_sda_clocks = I2C_RECOVERY_CLOCKS;

  EXPECT_TRUE(i2cRecoverBus(bus));

  EXPECT_EQ(1, bus.stops);
}

TEST(I2CBusRecovery, GivesUpOnSdaHeldPastNineClocks) {
  FakeBus bus;
  bus.target_sda_stuck = true;

  EXPECT_FALSE(i2cRecoverBus(bus));

  EXPECT_EQ(I2C_RECOVERY_CLOCKS, bus.rising_edges);
  EXPECT_FALSE(bus.ctl_scl_low);  // the controller doesn't leave the bus pulled low
}

TEST(I2CBusRecovery, GivesUpOnSclHeldLow) {
  FakeBus bus;
  bus.target_scl_stuck = true;

  EXPECT_FALSE(i2cRecoverBus(bus));

  EXPECT_EQ(0, bus.rising_edges);
  EXPECT_LE(bus.half_periods, I2C_RECOVERY_STRETCH_HALF_PERIODS + 1);  // bounded, not a hang
}

TEST(I2CBusRecovery, WaitsOutABriefClockStretch) {
  FakeBus bus;
  bus.scl_stretch_half_periods = 10;
  bus.target_sda_clocks = 2;

  EXPECT_TRUE(i2cRecoverBus(bus));

  EXPECT_TRUE(bus.sda());
  EXPECT_TRUE(bus.scl());
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
