# Changelog

All notable changes to this repository are listed here. Versions follow [Semantic Versioning](https://semver.org/).
Entries under **Breaking** need source changes in code that used the affected API; any of them calls for a major
version bump (a minor bump before 1.0).

## [Unreleased]

### Added

- MCP970x driver, first version: the MCP9700, MCP9700A, MCP9700B, MCP9701 and MCP9701A analog temperature sensors
  as rows of one variant table, plus an explicit `Custom` variant for other first-order linear sensors.
- Integer core: milli-degrees and millivolts, one optional `ToCelsius()` helper; the standard millivolt ADC hook.
- Reading checks: dummy conversions, averaging, clipped ADC (`Saturated`), noise (`Unstable`), range (`OutputLow`,
  `OutputHigh`); one-point calibration; `Sleep()` through an optional supply pin.
- Port types for a sensor on the supply and for one powered from a pin (ESP32 example), a host test of a `GpioSet()`
  with no pin behind it, and tests pinning the ground-fault floor of the default window per variant.
- Branch and condition coverage in the coverage gate output (`tools/ci-container/gates.sh`).
- Host tests against an independent sensor and ADC model with a differential oracle, a fuzz target, compile-fail
  tests, host and ESP32 examples, and the documentation with its figures.
