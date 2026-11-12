/**
 * @file port_without_delay.cpp
 * @brief Must NOT compile: the chip's reset and conversion waits need the port's DelayUs().
 *
 * EXPECT: DelayUs
 */
#include "mcp9700.hpp"
#include "mock_mcp9700.hpp"

class NoDelayPort : public mcp9700_test::MockPortBase<NoDelayPort> {};

int main() {
  NoDelayPort port;
  mcp9700::MCP9700<NoDelayPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
