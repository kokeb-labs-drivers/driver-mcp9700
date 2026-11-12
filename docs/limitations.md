# Limitations

What the driver does not do, what it assumes where the datasheet is silent, and what no test here can confirm. Every
assumption has a knob in `Config` and a model knob in the tests, and every open question has an owner decision or a
bench step. **The driver is not verified on hardware**: everything below is tested against a model written from the
datasheet, never against a real sensor.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Microchip Technology Inc. DS20001942 L:

    - Accuracy of each device: DC Electrical Characteristics, Sensor Accuracy, [p. 3](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=3)
    - Thermal response: DC Electrical Characteristics (t_RES), [p. 4](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=4)
    - Thermal considerations and self-heating: 4.4 Thermal Considerations, [p. 11](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=11)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Assumptions, each with a knob

| ID | Item | What the driver does | Config knob | Model knob and test |
| --- | --- | --- | --- | --- |
| A-1 | The turn-on time has a typical value (800 µs) and no maximum (p. 4) | Waits 2400 µs, three times the typical, always | `power_on_settle_us` | `turn_on_us`; `test_timing` |
| A-2 | The ADC's sample-and-hold settling after a channel switch is your hardware's | Discards one conversion before each reading | `discard_conversions` | `hold_ratio`; `test_faults` |
| A-3 | The noise of your board is not in the datasheet | Rejects a spread above 30 mV (3 °C on a 10 mV/°C part) | `max_spread_mv` | `noise_mv`; `test_boundaries` |
| A-4 | The accuracy is -4/+6 °C worst case (p. 3), and an ADC adds error | Accepts 8 °C beyond the specified range on each side | `range_margin_mc` | `test_boundaries` |
| A-5 | The output of an unpowered or switched-off sensor is not in the datasheet | Never relies on it; start-up checks the first reading | none | `unpowered_mv`; bench step in [Troubleshooting](troubleshooting.md) |
| A-6 | The thermal response is 1.3 s to 63 % (p. 4), so a long average is stale | Caps one reading at one second (a design decision) | `samples`, `sample_interval_us` | `kMaxAveragingWindowUs`; `test_validation` |
| A-7 | The ADC's full scale and resolution are yours; a wrong full scale hides a clip | Trusts the declaration; documents the effect | `adc_full_scale_mv`, `adc_resolution_bits` | `full_scale_mv`; `test_faults` |
| A-8 | Line regulation is 0.1 °C/V and self-heating 0.179 °C; the accuracy is tested at 3.3 V (MCP9700 family) or 5.0 V (MCP9701 family) | Does not correct them | none | `use_line_regulation`; `test_accuracy` |
| A-9 | The nonlinearity is ±0.5 °C typical over 0 to +70 °C | Leaves it in the result; AN1001 compensation is out of scope | none | `nonlinearity_c`; `test_model` |
| A-10 | The High-option accuracy rows cover -40 to +150 °C and the datasheet does not say an H part also meets the Extended rows | `High` uses the High rows alone over the whole range (the looser, safe reading); closed: no tighter guarantee is reported | none needed | `test_accuracy` |
| A-11 | A VOUT shorted to ground reads near 0 V plus the ADC's zero error | Accepts it when the ADC reports at or above the window floor (20 mV, 49 mV); [Configuration](configuration.md#the-ground-fault-floor) | `range_margin_mc` | `test_boundaries` |

## Open questions

| ID | Question | State |
| --- | --- | --- |
| O-1 | Keep the single floating-point helper `ToCelsius()`? | Kept: one pure function, the rest is integer |
| O-2 | The output of an unpowered or floating pin | Bench step, not verified ([Troubleshooting](troubleshooting.md#bench-steps-that-confirm-the-assumptions)) |
| O-3 | The MCP9700, MCP9700A and MCP9700B (and any `Custom`) are one transfer function to the driver | The declaration is trusted; only `AccuracyLimitsFor()` differs by grade |
| O-4 | `range = High` on a part that is not an H part accepts readings between +125 °C and +158 °C the part does not guarantee | The declaration is trusted; the table names the parts that have the option |
| O-5 | The ESP32 calibrated ADC ceiling at 12 dB (about 3.1 V) | The example declares 3100 mV; **not verified on hardware** |
| O-6 | Two-point calibration (a gain trim) | Not in this version; a future `gain_cal_ppm` pivoting on 25 °C is an additive change |
| O-7 | The datasheet's note 3 (p. 3) cites "Equation 4-2" for the first-order equation | A datasheet typo: Equation 4-1 (p. 10) is the transfer function, 4-2 is self-heating; the driver follows 4-1 |
| O-8 | The output load regulation cell lost its unit in text extraction | Read from the PDF: 2 Ω (p. 4); not used by the code |
| O-9 | The TO-92 body is drawn in inches, and its height row is named "bottom to package flat" | The millimetre values on [Hardware](hardware.md#package-outline) are conversions |
| O-10 | One repository for the whole family | Decided: the datasheet lists exactly five devices, held as table rows |
| O-11 | `Custom` is a declaration, not a verified part | Named variants are the default; `Custom` must declare everything and shares nothing with them |
| O-12 | A `Custom` whose output is 0 mV at its minimum cannot catch a grounded line | Documented below; a test asserts it so it is not mistaken for a defect |
| O-13 | `Custom` rejects a negative slope and a negative output at its minimum | Documented; an additive change if a part needs it |
| O-14 | A floating ADC input that holds a stable mid-scale level looks like a temperature | Documented under "What it does not do"; remedy is a board pull-down or a known-temperature start-up check; bench item |

## What it does not do

- **No correction for nonlinearity, supply or self-heating.** The result is the first-order equation of the
  datasheet; the error budget is on [Transfer function](transfer-function.md#the-error-budget). For better accuracy
  than ±0.5 °C typical after calibration, compensate with a table (the datasheet points to AN1001).
- **No gain trim.** One point calibrates the offset only.
- **No detection of a floating pin that holds a stable level.** A pin with no sensor that sits at a steady mid-scale
  voltage (a leaking ADC input, crosstalk, a held sample-and-hold capacitor) passes every check: 1650 mV reads as
  115 °C on an MCP9700. A stuck-at-0 line (`OutputLow`), a rail-high line (`Saturated`) and a moving line (`Unstable`) are
  caught. On the board, add a 100 kΩ pull-down at the ADC pin so an open line reads `OutputLow`, or compare the first
  reading with a known temperature (O-14). The range window is a property of the part, so it cannot be narrowed to the
  climate of one product without a knob; `range_margin_mc` shrinks it.
- **A grounded line is caught only below the window floor** (20 mV or 49 mV by default, A-11).
- **No detection of the part.** The five devices cannot be told apart at run time; the declaration is yours.
- **No detection of an ADC that is not what you declared.** A full scale that is too high hides a clip.
- **No power measurement.** The operating current (6 µA typical) is a documented value, not a reading.
- **No retries.** A reading that fails is reported once; call again if you want another.
- **Not thread-safe.** One owner, or one mutex.

## Custom parts

`Custom` serves another linear active thermistor IC whose output is first order. It is a declaration, not a verified
part: no test can know your sensor. A wrong V0, TC or range gives wrong temperatures that look plausible. The two
limits to know:

- **0 mV at the minimum (O-12).** If V0 + TC x min is exactly 0 mV, a reading of 0 mV is a valid reading at the
  minimum, so a grounded or unpowered line is not caught by the window at any margin. Check at a known temperature at
  start-up, or add a pull-up on the board.
- **Positive outputs only (O-13).** A part whose output goes below 0 V at its minimum, or whose slope is negative, is
  rejected by `Validate()`.

## Hardware

Not verified on hardware, and each item needs a bench:

| Item | Bench step |
| --- | --- |
| The model matches a real sensor | Compare `ReadRawPortMv()` at two known temperatures with the table on [Transfer function](transfer-function.md) |
| The turn-on time and the unpowered output | [Troubleshooting](troubleshooting.md#bench-steps-that-confirm-the-assumptions), steps 1 and 2 |
| The noise threshold of your board | Step 3 |
| The ESP32 ADC ceiling at 12 dB | Step 4 |
| The enable pin and the 200 Ω supply | Power the sensor from the pin, read after the settle wait, and compare with a sensor on the supply rail |
