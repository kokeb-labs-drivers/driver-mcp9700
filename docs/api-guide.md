# API guide

The driver's API by task, with code that compiles.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Microchip Technology Inc. DS20001942 L:

    - Shutdown from an I/O pin: 4.2 Shutdown Using Microcontroller I/O Pin, [p. 11](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=11)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

| Group | Calls |
| --- | --- |
| Lifecycle | `EnsureInitialized()`, `IsInitialized()`, `Deinitialize()`, `Sleep()` |
| Measurement | `ReadMeasurement()`, `ReadTemperatureMilliC()`, `ReadOutputMv()` |
| Bring-up | `ReadRawPortMv()`, `GetLastRawPortMv()` |
| Information | `GetConfig()`, `GetSpec()`, `ResolutionMilliC()` |
| Free functions | `Validate()`, `ResolveSpec()`, `OutputMvToMilliC()`, `MilliCToOutputMv()`, `AccuracyLimitsFor()`, `ToCelsius()`, `ToString()` |
| Presets | `ConfigFor()`, `FastPreset()`, `LowNoisePreset()`, `Mcp9701Preset()`, `HighTemperaturePreset()` |

The constructor stores the port and the configuration (`Config{}` unless you pass one) and does no I/O.
`EnsureInitialized()` is the one way to start the sensor: its first call checks the configuration, then starts it as the
[Contract](contract.md) page describes; once it has succeeded, later calls return `Ok` at once with no I/O, and after a
failure the next call starts again from the beginning. No other call starts the sensor: before `EnsureInitialized()`
succeeds every read returns `NotInitialized`. `Deinitialize()`, then `EnsureInitialized()`, is the recovery after a
fault.

## Read the temperature

```cpp
--8<-- "tests/docs/quickstart.cpp:quickstart"
```

`ReadTemperatureMilliC()` returns milli-degrees Celsius (25000 is 25.000 °C), after the calibration offset. The
result is an `int32_t`; print it with integer arithmetic, or call `ToCelsius()` once if a `float` suits you.

## The whole measurement

`ReadMeasurement()` returns the temperature, the mean voltage at VOUT and the spread of the conversions in one call.
That is the call to use when you log the voltage next to the temperature or watch the noise:

```cpp
--8<-- "tests/docs/api_tasks.cpp:measurement"
```

## Calibrate

```cpp
--8<-- "tests/docs/api_tasks.cpp:calibrated"
```

## Compare a reading with the datasheet

`AccuracyLimitsFor()` returns the error the datasheet guarantees for your part at a temperature. The driver never
applies it to a reading; it is for your own check, such as a production test against a reference:

```cpp
--8<-- "tests/docs/api_tasks.cpp:accuracy"
```

## Switch the sensor off

With an enable pin in the port, `Sleep()` powers the sensor down (a few µA less) and the next `EnsureInitialized()`
powers it up again and waits for it. Without the pin, `Sleep()` returns `Unsupported`. `Deinitialize()` does the same
and is also the recovery after a fault.

If `Sleep()` or `Deinitialize()` returns `Bus`, the pin write failed: the sensor may still be powered, and the driver is
uninitialised either way, so a second `Deinitialize()` does nothing. To retry the switch-off, call `EnsureInitialized()`
(which switches the sensor on and checks it) and then `Sleep()` again; the order is covered by `test_faults`.

## Look at the raw ADC

`ReadRawPortMv()` makes one conversion and returns the millivolts the port reported: no discard, no averaging, no
check. Use it at bring-up to see what your ADC does; `GetLastRawPortMv()` returns the last conversion any read made.

## Handle errors

```cpp
--8<-- "tests/docs/api_tasks.cpp:errors"
```

This analog driver never returns `Timeout`, `Crc`, `Busy` or `DeviceFault`: it has no bus protocol and no fault
register. Its faults are `OutputLow`, `OutputHigh`, `Saturated` and `Unstable`. A failed reading does not
uninitialise the driver, and no call retries: the next call is a new reading. Every value it returns is on the
[Contract](contract.md#errors) page, and the causes are in [Troubleshooting](troubleshooting.md).
