/**
 * @file not_a_port.cpp
 * @brief Must NOT compile: the driver is constrained by the port concept.
 *
 * EXPECT: Port
 */
#include "mcp9700.hpp"

struct NotAPort {
  bool EnsureInitialized() noexcept {
    return true;
  }
};

int main() {
  NotAPort port;
  mcp9700::MCP9700<NotAPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
