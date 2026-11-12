# Troubleshooting

Symptoms, causes and fixes, cheapest check first.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Microchip Technology Inc. DS20001942 L:

    - Output voltage range and an output outside it: 3.2 Output Voltage Pin (VOUT), [p. 9](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=9)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

Start with `ReadRawPortMv()`: it makes one conversion with no checks, so the number it returns is what your ADC sees.
Compare it with a multimeter on VOUT. If they differ, the problem is the port or the wiring, not the driver.

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| `EnsureInitialized()` returns `InvalidArgument` | `Validate()` failed: a field out of range, a named variant with a `custom` field set, `High` on a part without it, an incomplete `Custom` | Call `mcp9700::Validate(config)` and compare with [Configuration](configuration.md); nothing was sent |
| `EnsureInitialized()` returns `Bus` | The port's ADC bring-up, a conversion or the supply pin failed | Check the unit, the channel and the calibration in the port; the log line names the step |
| `EnsureInitialized()` returns `DeviceIdMismatch` | No plausible, stable, unclipped output: the log line before it says which check failed | See the four rows below |
| `OutputLow` (or "start-up output too low") | The sensor is unpowered, VOUT is grounded or open, the wrong channel, or the supply pin is not driven | Measure VDD and VOUT; check `Config::channel`; with an enable pin check the port drives it ACTIVE |
| `OutputHigh` ("too high") | VOUT shorted to the supply, a wrong variant (an MCP9701 declared as an MCP9700), or a mis-scaled ADC | Check the part marking against `Config::variant`; check a divider is declared |
| `Saturated` ("at the ADC full scale") | The ADC clipped: an open input pulled high, an attenuation too small, or a declared full scale that is too high | Measure the pin; declare the port's real full scale; for a floating input fix the wiring |
| `Unstable` ("unstable") | A floating or disturbed line, a noisy supply, or `max_spread_mv` too small for your ADC | Add the 100 nF at VDD (or 200 Ω and 1 µF); keep digital lines away; raise `samples` or `max_spread_mv` |
| The first reading after a wake-up is wrong | The sensor was not yet on | Raise `power_on_settle_us`; the datasheet gives no maximum turn-on time (p. 4) |
| The temperature is a few degrees off | The part's own accuracy (±1 °C to ±4 °C), the ADC's error, or a wrong variant | Compare with `AccuracyLimitsFor()`; calibrate once with `offset_cal_mc` ([Configuration](configuration.md#calibration)) |
| The temperature follows the board, not the air | The die is coupled to the PCB through the pins (section 4.4) | Use the TO-92 package for air, or thermally isolate the sensor |
| The temperature creeps up after a few seconds | Self-heating, or heat from a nearby regulator | An MCP970x at the maximum load self-heats 0.179 °C; look for the other heat source |
| The temperature is wrong by a constant ratio | A divider that is not declared, or the wrong `adc_full_scale_mv` | Declare `divider_num` and `divider_den`; compare `ReadRawPortMv()` with a multimeter |
| `Sleep()` returns `Unsupported` | The port has no `GpioSet()` | Add it ([Porting](porting.md#control-pins)) |
| `ReadMeasurement()` returns `NotInitialized` | `EnsureInitialized()` has not succeeded, or `Sleep()` or `Deinitialize()` ran | Call `EnsureInitialized()` and check its result |
| A custom part reads 0 °C with the input grounded | The part outputs 0 mV at its minimum, so 0 mV is valid | Open question O-12 in [Limitations](limitations.md#custom-parts) |

## Bench steps that confirm the assumptions

These are not verified on hardware. Each one turns an assumption into a measured fact for your board.

1. **Unpowered output.** The datasheet does not give the output of an unpowered or switched-off sensor (A-5). With the
   enable pin driven INACTIVE, read `ReadRawPortMv()` a few times, and write down the value and the spread. Then call
   `EnsureInitialized()` and confirm it succeeds only after the settle wait.
2. **Turn-on time.** Power the sensor and read `ReadRawPortMv()` every 100 µs: the time until the value stops moving is
   your board's turn-on time. The default wait is 2400 µs, three times the 800 µs typical (A-1).
3. **Noise.** Read 64 conversions with `max_spread_mv = 1000` and look at `Measurement::spread_mv`: set `max_spread_mv`
   to about three times your board's quiet value (A-3).
4. **Clip level.** Declare a full scale of 10000 mV, read a conversion with the pin tied to the supply, and write
   down what the port returns: that is the ceiling to declare (O-5).
5. **Floating pin.** Disconnect VOUT and read: an input with no pull is `Unstable` or reads a leakage level, and a
   level that holds steady anywhere inside the window is accepted as a temperature (O-14). A 100 kΩ pull-down
   (about 7 µA at 750 mV, far below the 100 µA the output can source) makes an open line read `OutputLow`.
