/**
 * @file test_errors.cpp
 * @brief The `Error` enum follows the standard, every value is returned by a
 * real call, and the values this analog driver never returns stay unreturned.
 */
#include <array>
#include <cstddef>
#include <cstring>

#include "mcp9700.hpp"
#include "test_harness.hpp"
#include "test_util.hpp"

using mcp9700::Error;
using mcp9700_test::FullPort;
using mcp9700_test::MinimalPort;

static_assert(static_cast<int>(Error::Ok) == 0 && static_cast<int>(Error::Bus) == 3 &&
                  static_cast<int>(Error::DeviceIdMismatch) == 5 &&
                  static_cast<int>(Error::Unsupported) == 9 &&
                  static_cast<int>(Error::OutputLow) == 16 &&
                  static_cast<int>(Error::Unstable) == 19,
              "the Error values are fixed (standards/driver.md)");

namespace {

constexpr int kFirstChipSpecific = 16;
/// Which Error values the tests have seen, by value.
std::array<bool, 256>& Seen() {
  static std::array<bool, 256> seen{};
  return seen;
}

void Record(Error e) {
  Seen().at(static_cast<uint8_t>(e)) = true;
}

void RunScenarios() {
  mcp9700::Config bad{};
  bad.samples = 0;
  {
    MinimalPort port;
    mcp9700::MCP9700<MinimalPort> s(port, bad);
    Record(s.EnsureInitialized().error); // InvalidArgument
  }
  {
    MinimalPort port;
    mcp9700::MCP9700<MinimalPort> s(port);
    Record(s.ReadMeasurement().error);   // NotInitialized
    Record(s.EnsureInitialized().error); // Ok
    Record(s.Sleep().error);             // Unsupported
    port.state.temperature_mc = -100'000;
    Record(s.ReadMeasurement().error); // OutputLow
    port.state.temperature_mc = 200'000;
    Record(s.ReadMeasurement().error); // OutputHigh
    port.state.temperature_mc = 25'000;
    port.state.fault = mcp9700_test::Fault::FloatHigh;
    Record(s.ReadMeasurement().error); // Saturated
    port.state.fault = mcp9700_test::Fault::FloatingDrift;
    Record(s.ReadMeasurement().error); // Unstable
  }
  {
    MinimalPort port;
    port.state.bus_up_ok = false;
    mcp9700::MCP9700<MinimalPort> s(port);
    Record(s.EnsureInitialized().error); // Bus
  }
  {
    MinimalPort port;
    port.state.wired_channel = 5;
    mcp9700::MCP9700<MinimalPort> s(port);
    Record(s.EnsureInitialized().error); // DeviceIdMismatch
  }
}

} // namespace

TEST_CASE("Every chip-specific Error is returned, and the four base values "
          "never are") {
  RunScenarios();
  for (int v = 0; v < 256; ++v) {
    const auto e = static_cast<Error>(v);
    if (std::strcmp(mcp9700::ToString(e), "Unknown") == 0) {
      continue;
    }
    const bool never = (e == Error::Timeout) || (e == Error::Crc) || (e == Error::Busy) ||
                       (e == Error::DeviceFault);
    CHECK(Seen().at(static_cast<std::size_t>(v)) != never);
    CHECK((v < 10) || (v >= kFirstChipSpecific));
  }
}

TEST_CASE("ToString names every value, and an out-of-enum value is Unknown") {
  const Error all[] = {Error::Ok,          Error::NotInitialized, Error::InvalidArgument,
                       Error::Bus,         Error::Timeout,        Error::DeviceIdMismatch,
                       Error::Crc,         Error::Busy,           Error::DeviceFault,
                       Error::Unsupported, Error::OutputLow,      Error::OutputHigh,
                       Error::Saturated,   Error::Unstable};
  for (const Error e : all) {
    CHECK(std::strcmp(mcp9700::ToString(e), "Unknown") != 0);
  }
  CHECK(std::strcmp(mcp9700::ToString(static_cast<Error>(10)), "Unknown") == 0);
  CHECK(std::strcmp(mcp9700::ToString(static_cast<Error>(200)), "Unknown") == 0);
  CHECK(std::strcmp(mcp9700::ToString(Error::OutputHigh), "OutputHigh") == 0);
}

TEST_CASE("Result helpers") {
  const auto ok = mcp9700::Result<int>::Success(3);
  CHECK(ok.ok() && static_cast<bool>(ok));
  const auto bad = mcp9700::Result<int>::Failure(Error::Bus);
  CHECK(!bad.ok() && !static_cast<bool>(bad) && bad.value == 0);
  CHECK(mcp9700::Result<void>::Success().ok());
  CHECK(!mcp9700::Result<void>::Failure(Error::Bus).ok());
  CHECK(static_cast<bool>(mcp9700::Result<void>::Success()));
}
