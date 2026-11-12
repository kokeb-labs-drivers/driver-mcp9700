# Contract

What the MCP970x driver promises, what it needs, and the rules a caller follows.

The driver inherits the [Kokeb Labs driver contract](https://github.com/kokeb-labs/.github/blob/main/standards/driver.md);
this page states only what is specific to it.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Microchip Technology Inc. DS20001942 L:

    - Output voltage range and an output outside it: 3.2 Output Voltage Pin (VOUT), [p. 9](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=9)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Promises

- The constructor is `noexcept`, stores the configuration, does no I/O and cannot fail.
- `EnsureInitialized()` checks the configuration first (`InvalidArgument` means nothing was sent), brings the ADC
  up, powers the sensor when the port has `GpioSet()`, waits `power_on_settle_us` and takes one validated reading.
  It is the only start-up call, and after it has succeeded it returns `Ok` with no I/O.
- Every call that touches the sensor returns `Error::NotInitialized` before `EnsureInitialized()` succeeds, and
  again after `Deinitialize()` or `Sleep()`, with no conversion and no pin write. No call starts the sensor as a side
  effect.
- A reading sends exactly `discard_conversions + samples` conversions, waits `sample_interval_us` between them and
  after none, and never retries.
- A reading is never clamped: a value outside the window is an error, not a limited temperature.
- A failed reading does not uninitialise the driver; a failed start-up leaves it uninitialised, with the sensor
  switched off again when the port has `GpioSet()`, and the next `EnsureInitialized()` starts again.
- Every port failure propagates as `Error::Bus`. A failed call returns a zero value.
- `ReadRawPortMv()` sends one conversion, and returns what the port gave: no scaling, no check, no retry.
- The sensor is driven INACTIVE (unpowered) by `Deinitialize()` and `Sleep()`, and ACTIVE only by
  `EnsureInitialized()`.
- Every log line is non-empty plain text, never a format string.

## Depends on

| Dependency | Version | Why |
| --- | --- | --- |
| A C++20 compiler | GCC 10, Clang 13 or later | Concepts constrain the port |
| CMake | 3.16 or later | Generates `mcp9700_version.h` |
| Your port | | The ADC in millivolts, the delay, optionally the supply pin and a log |

## Threading

Not thread-safe; takes no lock. One owner task, or one mutex around every call.

## Errors

| `Error` | Value | Returned when |
| --- | --- | --- |
| `Ok` | 0 | The call succeeded |
| `NotInitialized` | 1 | Before `EnsureInitialized()` succeeded, or after `Deinitialize()` or `Sleep()` |
| `InvalidArgument` | 2 | `Validate()` failed at `EnsureInitialized()`; nothing was sent |
| `Bus` | 3 | The port's ADC bring-up, a conversion or a pin write failed |
| `Timeout` | 4 | Never returned |
| `DeviceIdMismatch` | 5 | Start-up found no plausible, stable, unclipped output on the channel |
| `Crc` | 6 | Never returned |
| `Busy` | 7 | Never returned |
| `DeviceFault` | 8 | Never returned |
| `Unsupported` | 9 | `Sleep()` on a port without `GpioSet()` |
| `OutputLow` | 16 | The temperature is below the specified range minus the margin |
| `OutputHigh` | 17 | The temperature is above the specified range plus the margin |
| `Saturated` | 18 | A conversion is within one ADC step of the declared full scale, or above it |
| `Unstable` | 19 | The conversions of one reading differ by more than `max_spread_mv` |

The base set keeps its values 0 to 9; the four chip-specific values start at 16, and each is returned by a call the
tests make. `Timeout`, `Crc`, `Busy` and `DeviceFault` stay in the enumeration so a port can share one error type
across drivers, and a test asserts that this driver never returns them. During `EnsureInitialized()`, `OutputLow`,
`OutputHigh`, `Saturated` and `Unstable` are reported as `DeviceIdMismatch` with the reason logged; after it, a
reading reports them as they are.

When more than one applies, the order is `Bus`, `Saturated`, `Unstable`, then `OutputLow` or `OutputHigh`.

## Memory

No heap, exceptions, RTTI or static mutable state. The object holds a port reference, a `Config`, the resolved
variant row and the last conversion, a few hundred bytes. The driver has no floating-point except the optional
`ToCelsius()`.

## Platform

Platform-free; the ESP-IDF port in `examples/esp32` is an example.

## Deviations from the standard

None.
