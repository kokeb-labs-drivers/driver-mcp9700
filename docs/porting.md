# Porting

Run the driver on a new MCU by writing one class: the port. It moves one millivolt value from your ADC and waits;
the driver does everything else.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Microchip Technology Inc. DS20001942 L:

    - Absolute maximum ratings: 1.0 Electrical Characteristics, [p. 3](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=3)
    - Output current, impedance and load capacitance: DC Electrical Characteristics (I_OUT, Z_OUT, C_LOAD), [p. 4](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=4)
    - Shutdown from an I/O pin: 4.2 Shutdown Using Microcontroller I/O Pin, [p. 11](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=11)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

The driver calls hooks on a class you derive from `mcp9700::AdcInterface<Self>` (in
[`inc/mcp9700_adc_interface.hpp`](../inc/mcp9700_adc_interface.hpp)). The base declares no hook; the driver template
is constrained by the `mcp9700::AdcPort` concept, so a missing or misdeclared hook is a compile error.

## What you implement

| Method | Required | Contract |
| --- | --- | --- |
| `bool EnsureInitialized() noexcept` | yes | Bring the ADC up (unit, channel attenuation, calibration) on the first call; `true` at once afterwards |
| `bool ReadVoltageMv(uint8_t channel, uint32_t& voltage_mv) noexcept` | yes | One conversion of `channel`, **calibrated, in millivolts at the ADC pin**; `false` on any failure, leaving `voltage_mv` untouched |
| `void DelayUs(uint32_t us) noexcept` | yes, for every wait | Wait at least `us` microseconds |
| `bool GpioSet(CtrlPin, GpioSignal) noexcept` | no | Power the sensor from an MCU pin; without it `Sleep()` returns `Unsupported` |
| `void Log(LogLevel, const char* message) noexcept` | no | Print one line; `message` is plain text, never a format string |

`ReadVoltageMv()` is the standard millivolt hook of the Kokeb Labs driver contract, so one ADC port serves every
analog-input driver. The driver asks for no raw counts and no reference voltage: you tell it the full scale and the
resolution of your port in `Config` ([Configuration](configuration.md#the-adc)).

## The port

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

A port with every hook:

```cpp
--8<-- "tests/docs/ports.cpp:full-port"
```

## What the ADC must do

| Limit | Value | Why |
| --- | --- | --- |
| Input current | 100 µA at most (`kMaxOutputCurrentUa`) | The sensor sources and sinks at most that (I<sub>OUT</sub>, p. 4) |
| Source impedance of the sensor | 20 Ω typical (`kOutputImpedanceOhm`) | An ADC with a large sample capacitor sees a small error after a switch |
| Load capacitance | 1000 pF (`kMaxLoadCapacitancePf`) | The family is characterised and production tested with that (p. 4) |
| Series resistor in VOUT | none | It adds an error with the input current; filter in the supply line instead (section 4.3) |

- **Return calibrated millivolts.** If the SDK has a calibration (a curve fit for the ESP32, a factory gain and offset
  elsewhere), apply it in the port. If it has none, fail `EnsureInitialized()` rather than scale by a guess: a count
  scaled by a guess looks like a voltage and is not one.
- **Declare the ceiling.** On some MCUs the calibrated range at the attenuation you use ends below the supply (about
  3.1 V at 12 dB on ESP32 parts). Put that value in `Config::adc_full_scale_mv`, not the supply: the clip check
  compares conversions with it. The ESP32 port keeps it as a constant next to the attenuation and asserts both at
  compile time.
- **Multiplexed ADCs.** The sample capacitor holds the last channel for a moment after a switch; that is what the
  driver's dummy conversion is for. If you give the sensor an ADC of its own, set `discard_conversions` to 0.
- **Wait in `DelayUs()`.** Power-up and sample spacing depend on it. A busy-wait is fine for the microseconds the
  driver asks; it never asks for more than a second at a time.

## Control pins

The sensor may be powered from an MCU pin (section 4.2, p. 11), with 200 Ω in the supply line and 1 µF at the sensor
(section 4.3; see [Hardware](hardware.md#power-from-an-io-pin)). Each control pin has a defined idle level, and the
port drives it there before it enables the pin's output (handbook KB-HW-018): configuring a pin as an output first and
setting its level afterwards can glitch the supply.

| `CtrlPin` | Package pin | Polarity | Idle (`INACTIVE`) level | Before the first read |
| --- | --- | --- | --- | --- |
| `Enable` | VDD, through 200 Ω | `ACTIVE` = powered (pin high) | low: sensor off | Set low, then enable the output; `EnsureInitialized()` powers it |

`GpioSignal::ACTIVE` always means "function asserted"; the port maps it to the electrical level. With an enable pin
the driver gains `Sleep()`, and a failed start-up switches the sensor off again.

## Ports in this repository

| Port | Where |
| --- | --- |
| ESP-IDF: `adc_oneshot` with curve-fitting calibration, an optional enable GPIO, `esp_rom_delay_us` | [`esp32_mcp9700_port.hpp`](../examples/esp32/main/esp32_mcp9700_port.hpp) |
| Host: the simulated sensor and ADC | [`host_mcp9700_port.hpp`](../examples/host/host_mcp9700_port.hpp) |

## Threads

The driver is not thread-safe. Give each driver object one owner task, or guard every call with one mutex.
