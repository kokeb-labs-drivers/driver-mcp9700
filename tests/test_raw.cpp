/**
 * @file test_raw.cpp
 * @brief Single-shot raw access and the last-conversion getter (handbook
 * KB-CPP-129).
 */
#include "mcp9700.hpp"
#include "test_harness.hpp"
#include "test_util.hpp"

using mcp9700::Error;

TEST_CASE("ReadRawPortMv: one conversion, unscaled, unchecked; "
          "GetLastRawPortMv follows every "
          "conversion") {
  mcp9700_test::ScriptPort port;
  mcp9700::Config c{};
  c.divider_num = 2;
  c.divider_den = 1;
  mcp9700::MCP9700<mcp9700_test::ScriptPort> sensor(port, c);
  port.Set(375);
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK_EQ(sensor.GetLastRawPortMv(), 375U);
  const int before = port.conversions;
  port.Set(0xFFFF'FFFFU);
  const auto raw = sensor.ReadRawPortMv();
  REQUIRE(raw.ok());
  CHECK_EQ(raw.value,
           0xFFFF'FFFFU); // whatever the port gave, not scaled, not checked
  CHECK_EQ(port.conversions - before, 1);
  CHECK_EQ(sensor.GetLastRawPortMv(), 0xFFFF'FFFFU);
  port.Set(0);
  CHECK_EQ(sensor.ReadMeasurement().error, Error::OutputLow);
  CHECK_EQ(sensor.GetLastRawPortMv(), 0U);
  port.Set(400);
  REQUIRE(sensor.ReadRawPortMv().ok());
  CHECK_EQ(sensor.GetLastRawPortMv(), 400U);
  port.fail_at = port.conversions;
  CHECK_EQ(sensor.ReadRawPortMv().error, Error::Bus);
  CHECK_EQ(sensor.GetLastRawPortMv(), 0U); // a failed conversion leaves no stale value
  port.fail_at = -1;
  REQUIRE(sensor.ReadRawPortMv().ok());
  CHECK_EQ(sensor.GetLastRawPortMv(), 400U);
  REQUIRE(sensor.Deinitialize().ok());
  CHECK_EQ(sensor.GetLastRawPortMv(), 0U); // and a stopped driver reports none
}
