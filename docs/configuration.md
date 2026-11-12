# Configuration

Every field of `mcp9700::Config`, the variant table, the presets, the `Custom` declaration, calibration and the
optional Kconfig layer. `Validate()` checks all of it, without touching the hardware, and `EnsureInitialized()`
calls it before any I/O.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Microchip Technology Inc. DS20001942 L:

    - Supply range and current: DC Electrical Characteristics, [p. 3](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=3)
    - Accuracy of each device: DC Electrical Characteristics, Sensor Accuracy, [p. 3](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=3)
    - Turn-on time: DC Electrical Characteristics (t_ON), [p. 4](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=4)
    - Specified temperature ranges: Temperature Characteristics, [p. 4](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=4)
    - Thermal response: DC Electrical Characteristics (t_RES), [p. 4](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=4)
    - Transfer function: V0 and TC of each device: 4.0 Applications Information, Equation 4-1; DC Electrical Characteristics, [p. 10](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=10)
    - Improving accuracy: a one-point calibration: 4.1 Improving Accuracy, [p. 10](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=10)
    - The five devices and the packages each comes in: Product Identification System; 5.1 Package Marking, [p. 23](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=23)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## The fields

| Field | Default | Range | What it is |
| --- | --- | --- | --- |
| `channel` | 0 | 0 to 255 | Passed to `ReadVoltageMv()` unchanged |
| `variant` | `Mcp9700` | the five named parts, or `Custom` | The part on the board; a declaration, never detected |
| `range` | `Extended` | `Extended`, or `High` where the part has it | The upper end of the specified range |
| `custom` | all zero | see [Custom](#a-custom-sensor) | Only with `Variant::Custom`; must be all zero otherwise |
| `adc_full_scale_mv` | 3300 | 100 to 10000 | The largest voltage your port reports at the pin |
| `adc_resolution_bits` | 12 | 8 to 16 | The resolution of your ADC |
| `divider_num`, `divider_den` | 1, 1 | 1 to 65535 each | VOUT = pin x `divider_num` / `divider_den` |
| `offset_cal_mc` | 0 | -20000 to 20000 | Added to the result: a one-point calibration |
| `range_margin_mc` | 8000 | 0 to 20000 | The plausibility margin on each side of the range |
| `samples` | 8 | 1 to 64 | Conversions averaged per reading |
| `sample_interval_us` | 100 | 0 to 100000 | Wait between conversions |
| `discard_conversions` | 1 | 0 to 4 | Dummy conversions before each reading |
| `max_spread_mv` | 30 | 0 to 1000 | The largest max minus min of one reading, at VOUT |
| `power_on_settle_us` | 2400 | 0 to 1000000 | Wait after the sensor is switched on |

The reading span `(discard + samples - 1) x sample_interval_us` may not exceed one second (`kMaxAveragingWindowUs`):
the sensor itself takes 1.3 s to reach 63 % of a step (t<sub>RES</sub>, p. 4), so an average over more than that is
staler than the sensor. Every limit is a named constant in [`mcp9700_types.hpp`](../inc/mcp9700_types.hpp), and
`Validate()` is tested one step inside and one step outside each.

## The variant table

The five devices are rows of `mcp9700::kVariantTable`: V0, TC, the specified range (and the High range), the supply,
the accuracy bands and the packages ([Hardware](hardware.md#the-family) shows them). Nothing in the code branches on
a part name. To use a named part, pick it:

```cpp
--8<-- "tests/docs/configuration.cpp:named"
```

The grades differ only in accuracy, which the driver reports through `AccuracyLimitsFor()` and does not apply. The
MCP9700, MCP9700A and MCP9700B therefore give the same temperature from the same voltage, and the choice among them
is yours to declare correctly (open question O-3). `range = High` is accepted for the MCP9700 and MCP9700B only; the
driver cannot know that your part was ordered with the `H` option, so with `High` on a part that is not an H part,
readings between +125 °C and +158 °C are accepted that the part does not guarantee (O-4).

## The ADC

`adc_full_scale_mv` and `adc_resolution_bits` describe your port, not the sensor. They matter for two checks:

- **The clip check.** A conversion within one ADC step of the full scale is `Saturated`. A wrong (too large) full
  scale hides a clipped ADC, because the clip is then never at the declared ceiling. Declare the largest calibrated
  voltage the port can report, which on some MCUs is below the supply (the ESP32 example uses 3100 mV at 12 dB).
- **The resolution.** `ResolutionMilliC()` reports one step in milli-degrees.

A voltage divider in front of the ADC is declared with `divider_num` and `divider_den`; the driver scales the
conversions back to the voltage at VOUT before it converts them. Only a divider that keeps the largest output under
the full scale makes sense (a 3.0 V MCP9701 output on a 2.5 V ADC would clip).

```cpp
--8<-- "tests/docs/configuration.cpp:divider"
```

## Sampling

`EnsureInitialized()` waits `power_on_settle_us` after it switches the sensor on. The datasheet gives a typical
turn-on time of 800 µs and no maximum (p. 4), so the default is three times the typical value, 2400 µs; a larger
value is cheap, and the start-up reading is checked anyway. The wait is never skipped, even when the port has no enable
pin: the sensor may have been powered a moment before.

A reading is `discard_conversions` dummy conversions, then `samples` conversions with `sample_interval_us` between
every two conversions (none after the last). A multiplexed ADC's sample capacitor holds the previous channel for a
moment after a switch, so the default discards one. The mean of the real conversions is the output voltage; their
spread (largest minus smallest, at VOUT) must not exceed `max_spread_mv`, default 30 mV, which is 3 °C on a 10 mV/°C
part. These defaults are assumptions of this driver, not datasheet values (A-1 to A-3); each has a field so you can
change it.

| Preset | What it sets |
| --- | --- |
| `FastPreset()` | One conversion per reading, no wait |
| `LowNoisePreset()` | 32 conversions, 200 µs apart (6.2 ms), against ADC noise and mains pickup |
| `Mcp9701Preset()` | The MCP9701 transfer function and range |
| `HighTemperaturePreset()` | An MCP9700 with the High option, to +150 °C |
| `ConfigFor(variant)` | `Config{}` with that variant |

## The window

After the conversions are averaged and converted, the temperature must lie in the part's specified range widened by
`range_margin_mc` on each side. Below it is `OutputLow`, above it `OutputHigh`. The default margin of 8 °C is the
datasheet's worst-case accuracy (-4 °C to +6 °C, p. 3) plus 2 °C for the ADC (A-4); a larger margin makes the check
more forgiving, 0 makes it strict. The calibration offset is added **after** this check, so a calibrated reading
can sit up to `kMaxOffsetCalMc` beyond an edge.

### The ground-fault floor

The low edge of the window is also the highest voltage at which a VOUT shorted to ground is still accepted, because the
driver cannot tell a grounded line from a very cold sensor below that voltage. It is `v0 + tc x (minimum - margin)`:

| Variant | Lowest valid output | Floor at the default 8 °C margin | Floor at 2 °C |
| --- | --- | --- | --- |
| MCP9700, MCP9700A, MCP9700B (-40 °C) | 100 mV | 20 mV | 80 mV |
| MCP9701, MCP9701A (-10 °C) | 205 mV | 49 mV | 166 mV |

A grounded line is therefore recognised only if the ADC reads below the floor. An ADC with a positive zero error of
more than 20 mV (49 mV) reports a grounded pin above it and the driver reads about -47 °C. The default margin stays at
8 °C because it also has to absorb the part's worst-case error (-4/+6 °C) and the ADC's error at the bottom of its
range, which is large on some converters. Where you know your ADC's zero error, lower `range_margin_mc` (2000 puts the
floor at 80 mV and 166 mV) and keep the margin at or above the part's error at the cold end. `test_boundaries`
pins every floor in the table.

## Calibration

A one-point calibration removes a fixed error of the part, the ADC and the board together (section 4.1, p. 10):

1. Put the sensor at a known temperature, for example +25 °C from a reference thermometer, and let it settle.
2. Read `ReadTemperatureMilliC()` with `offset_cal_mc = 0`.
3. Set `offset_cal_mc` to the reference minus the reading. The example below reads 1.2 °C high, so the offset is
   -1200.

```cpp
--8<-- "tests/docs/api_tasks.cpp:calibrated"
```

The offset applies to every later reading; it moves the result, not the window. A fixed offset cannot correct a
slope error.

## A Custom sensor

`Variant::Custom` describes another linear active thermistor IC whose output is first order, V = V0 + TC x T, from
data you take from **its own datasheet**. The driver does not know the part: it checks that the declaration is
consistent, and cannot check that it is true. A wrong value gives temperatures that look plausible and are wrong,
and `EnsureInitialized()` logs one warning (`Log()` at `Warning`) saying so.

| `Config::custom` field | Meaning | Limits |
| --- | --- | --- |
| `v0_mv` | Output at 0 °C, millivolts | 0 to 5000 |
| `tc_uv_per_c` | Slope, microvolts per °C | 1000 to 100000, positive only |
| `min_mc`, `max_mc` | The specified range, milli-degrees | each within ±200000, `min_mc` below `max_mc` |
| `accuracy_min_mc`, `accuracy_max_mc` | The part's worst-case error over that range | each within ±20000, they bracket 0, not both 0 |

The output must stay between 0 and 20000 mV at both ends of the range. `range` must stay `Extended`; the other
`Config` fields (ADC, sampling, margin, calibration) apply as for a named part. Every field must be set: an
all-zero declaration fails `Validate()`, and so does any non-zero field on a named variant, so a half-edited
configuration cannot silently run as a different part.

To fill it in, take four numbers from the datasheet's transfer function and electrical tables. If the datasheet gives
the output at two temperatures instead, compute TC = (V2 - V1) / (T2 - T1) in µV/°C and V0 = V1 - TC x T1. Here is a
part that gives 750 mV at 0 °C and 10 mV/°C, specified from -40 to +125 °C with an error between -2 and +3 °C. The
output at the two ends is 750 - 400 = 350 mV and 750 + 1250 = 2000 mV, both inside the window:

```cpp
--8<-- "tests/docs/configuration.cpp:custom-offset"
```

A part that outputs 0 mV at its minimum temperature needs a second look. This one gives 0 mV at 0 °C and is specified
from 0 °C up:

```cpp
--8<-- "tests/docs/configuration.cpp:custom-zero"
```

At its own minimum such a part outputs 0 mV, so a 0 mV reading is a valid reading of 0 °C, and a grounded or
unpowered line cannot be told from it by the window at any margin (open question O-12). Check such a part at start-up
at a temperature you know, or add a pull-up on the board. The driver also rejects a negative slope and a negative
output at `min_mc` (O-13): a part that needs a negative supply could not be read by an ADC anyway. If you need one, that
is an additive change; open an issue.

The named parts and the `Custom` declaration share nothing: a `Custom` is never a tweak of a named variant.

## Kconfig

With ESP-IDF, `idf.py menuconfig` has a "MCP970x analog temperature sensor" menu, and
`mcp9700::KconfigDefaults()` builds a `Config` from it:

```cpp
--8<-- "tests/docs/configuration.cpp:kconfig"
```

Outside ESP-IDF the function returns `Config{}`. Every value reaches the code only through
[`mcp9700_kconfig.hpp`](../inc/mcp9700_kconfig.hpp).

| Option | Default | `Config` field |
| --- | --- | --- |
| `CONFIG_MCP9700_ADC_CHANNEL` | 0 | `channel` |
| `CONFIG_MCP9700_VARIANT_*` | `MCP9700` | `variant` (a choice: the five parts or `CUSTOM`) |
| `CONFIG_MCP9700_HIGH_TEMPERATURE` | n | `range` (`High` when set) |
| `CONFIG_MCP9700_CUSTOM_V0_MV` and the other `CUSTOM_*` | 0 | `custom` (only with `CUSTOM`) |
| `CONFIG_MCP9700_ADC_FULL_SCALE_MV` | 3300 | `adc_full_scale_mv` |
| `CONFIG_MCP9700_SAMPLES` | 8 | `samples` |
| `CONFIG_MCP9700_SAMPLE_INTERVAL_US` | 100 | `sample_interval_us` |
| `CONFIG_MCP9700_OFFSET_CAL_MC` | 0 | `offset_cal_mc` |

The resolution is not a menu item: it belongs to your port, so set `adc_resolution_bits` in code next to the port's
full scale, as [`examples/esp32`](../examples/esp32/README.md) does.
