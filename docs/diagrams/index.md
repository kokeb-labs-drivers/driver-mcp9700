# Figures

<!-- Written by kokeb-docs figures from docs/diagrams/figures/*.py. Do not edit by hand. -->

Every figure on this site is drawn by a Python module from facts extracted from the code, so it is rebuilt,
not redrawn, when the code changes. CI runs `kokeb-docs figures --check` and fails when a
committed figure no longer matches the code. Each figure below lists the files it is drawn from.

## Inside the MCP9700

<!-- figure: chip-blocks -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-blocks.dark.svg">
  <img src="chip-blocks.svg"
    alt="Block diagram of the MCP9700: its function blocks, how they connect and which package pins reach
    each block.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/mcp9700.hpp`](../../inc/mcp9700.hpp)
- [`inc/mcp9700_adc_interface.hpp`](../../inc/mcp9700_adc_interface.hpp)

## What the MCP9700 does

<!-- figure: chip-capabilities -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-capabilities.dark.svg">
  <img src="chip-capabilities.svg"
    alt="Capability map of the MCP9700: Linear temperature output (supported): Turns the output voltage into
    milli-degrees with the datasheet's first-order equation, in integers; Five devices (supported): The
    MCP9700, 9700A, 9700B, 9701 and 9701A with their slopes, offsets, ranges and accuracy grades;
    Temperature classes (supported): Extended range, or +150 °C on the MCP9700 and MCP9700B High option;
    a reading outside it is refused; Direct ADC connection (supported): VOUT goes straight to an ADC
    pin; the driver averages conversions and refuses a clipped or noisy line; One-point calibration
    (supported): Removes the sensor's offset with one reading at a known temperature, as the datasheet
    advises; Power from an I/O pin (supported): Switches the sensor's supply from an MCU pin to save
    current; 800 µs typical turn-on; Nonlinearity compensation (not covered): A look-up table can bring
    the accuracy to ±0.5 °C typical; the driver leaves the ±0.5 °C curve in the result; Self-heating
    (not covered): 0.179 °C at the worst case at 25 °C; the driver does not correct it.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/mcp9700.hpp`](../../inc/mcp9700.hpp)
- [`inc/mcp9700_adc_interface.hpp`](../../inc/mcp9700_adc_interface.hpp)

## MCP9700: Powered from an I/O pin

<!-- figure: chip-circuit-gpio-powered -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-circuit-gpio-powered.dark.svg">
  <img src="chip-circuit-gpio-powered.svg"
    alt="Schematic of the MCP9700 powered from an i/o pin: the chip with U2 MCU, R1 resistor 200 Ω, C1
    capacitor 1 µF; the MCU wires carry the port hooks GpioSet(CtrlPin::Enable), ReadVoltageMv().">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/mcp9700.hpp`](../../inc/mcp9700.hpp)
- [`inc/mcp9700_adc_interface.hpp`](../../inc/mcp9700_adc_interface.hpp)

## MCP9700: Typical application

<!-- figure: chip-circuit-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-circuit-typical.dark.svg">
  <img src="chip-circuit-typical.svg"
    alt="Schematic of the MCP9700 typical application: the chip with U2 MCU, C1 capacitor 100 nF; the MCU
    wires carry the port hooks GpioSet(CtrlPin::Enable), ReadVoltageMv().">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/mcp9700.hpp`](../../inc/mcp9700.hpp)
- [`inc/mcp9700_adc_interface.hpp`](../../inc/mcp9700_adc_interface.hpp)

## MCP9700

<!-- figure: chip-hero -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-hero.dark.svg">
  <img src="chip-hero.svg"
    alt="Overview of the MCP9700, an analog temperature sensor in a three-pin package: its output voltage
    rises in a straight line with the temperature, 10.0 mV per degree from 500 mV at 0 °C, so an ADC
    input reads it with no other parts. A MCU talks to it over ADC input; it reads back Die temperature.
    Headline figures: 10.0 mV/°C slope (19.5 mV/°C for the MCP9701 and MCP9701A); 500 mV output at 0 °C
    (400 mV for the MCP9701 and MCP9701A); -40 to +125 °C measuring range (to +150 °C with the High
    option); ±1 to ±4 °C accuracy by grade, up to +70 °C; 6 µA typical supply current.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/mcp9700.hpp`](../../inc/mcp9700.hpp)
- [`inc/mcp9700_adc_interface.hpp`](../../inc/mcp9700_adc_interface.hpp)

## MCP9700 package, SOT-23-3

<!-- figure: chip-outline -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-outline.dark.svg">
  <img src="chip-outline.svg"
    alt="Outline of the MCP9700 in SOT-23-3 drawn to scale from above: body 2.9 x 1.3 x 1.12 mm, 3 pins.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/mcp9700.hpp`](../../inc/mcp9700.hpp)
- [`inc/mcp9700_adc_interface.hpp`](../../inc/mcp9700_adc_interface.hpp)

## MCP9700 pinout, SOT-23-3

<!-- figure: chip-pinout -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-pinout.dark.svg">
  <img src="chip-pinout.svg"
    alt="Pinout of the MCP9700 in SOT-23-3, top view: 3 pins, each with its name, its electrical type as a
    colour and the driver hook that uses it.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/mcp9700.hpp`](../../inc/mcp9700.hpp)
- [`inc/mcp9700_adc_interface.hpp`](../../inc/mcp9700_adc_interface.hpp)

## Wiring: Powered from an I/O pin

<!-- figure: chip-wiring-gpio-powered -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-wiring-gpio-powered.dark.svg">
  <img src="chip-wiring-gpio-powered.svg"
    alt="Wiring of the MCP9700 for the Powered from an I/O pin: MCU ADC input to VOUT (ReadVoltageMv()); MCU
    GPIO (enable) to VDD through R1 200 Ω (GpioSet(CtrlPin::Enable)). Supplies, grounds and the support
    parts are listed under the drawing.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/mcp9700.hpp`](../../inc/mcp9700.hpp)
- [`inc/mcp9700_adc_interface.hpp`](../../inc/mcp9700_adc_interface.hpp)

## Wiring: Typical application

<!-- figure: chip-wiring-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-wiring-typical.dark.svg">
  <img src="chip-wiring-typical.svg"
    alt="Wiring of the MCP9700 for the Typical application: MCU ADC input to VOUT (ReadVoltageMv()).
    Supplies, grounds and the support parts are listed under the drawing.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/mcp9700.hpp`](../../inc/mcp9700.hpp)
- [`inc/mcp9700_adc_interface.hpp`](../../inc/mcp9700_adc_interface.hpp)

## The layers, one of them yours

<!-- figure: driver-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="driver-layers.dark.svg">
  <img src="driver-layers.svg"
    alt="Layer diagram: your application calls the MCP9700 core, which sends register reads and writes
    through the ADC port contract to the port class you write, which drives the chip; the core and the
    contract are the same on every MCU.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/driver-layers.py`](../../docs/diagrams/figures/driver-layers.py)

Drawn from:

- `public calls` in [`inc/mcp9700.hpp`](../../inc/mcp9700.hpp)
- `AdcInterface` in [`inc/mcp9700_adc_interface.hpp`](../../inc/mcp9700_adc_interface.hpp)

## Power-on settling

<!-- figure: power-on-timing -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="power-on-timing.dark.svg">
  <img src="power-on-timing.svg"
    alt="Power-on of an MCP9700 supplied from an MCU pin: VDD rises, VOUT is undefined for the turn-on time,
    then follows the transfer function and the ADC may sample it.">
</picture>
<!-- /figure -->

Hardware description: [`docs/model/power-on-timing.timing.yml`](../model/power-on-timing.timing.yml)

Drawn from:

- [`docs/model/power-on-timing.timing.yml`](../model/power-on-timing.timing.yml)

## One reading, step by step

<!-- figure: read-pipeline -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="read-pipeline.dark.svg">
  <img src="read-pipeline.svg"
    alt="Flow of one reading: discard dummy conversions, convert and average N samples, check for a clipped
    ADC (Saturated), check the spread (Unstable), scale to VOUT and convert to a temperature, check the
    window (OutputLow, OutputHigh) and add the calibration offset. A failed conversion is Bus.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/read-pipeline.py`](../../docs/diagrams/figures/read-pipeline.py)

Drawn from:

- `collect()` in [`src/mcp9700.ipp`](../../src/mcp9700.ipp)
- `Evaluate()` in [`src/mcp9700.ipp`](../../src/mcp9700.ipp)
- `ReadMeasurement()` in [`src/mcp9700.ipp`](../../src/mcp9700.ipp)

## Output voltage against temperature

<!-- figure: transfer-function -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="transfer-function.dark.svg">
  <img src="transfer-function.svg"
    alt="Chart of VOUT in millivolts against temperature from -50 to +160 degrees Celsius: the MCP9700 family
    at 500 mV plus 10.0 mV per degree, the MCP9701 family at 400 mV plus 19.5 mV per degree, and a
    Custom example, each solid over its specified range and dotted over the plausibility margin.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/transfer-function.py`](../../docs/diagrams/figures/transfer-function.py)

Drawn from:

- `kVariantTable` in [`inc/mcp9700_types.hpp`](../../inc/mcp9700_types.hpp)
- `range_margin_mc` in [`inc/mcp9700_types.hpp`](../../inc/mcp9700_types.hpp)
- `custom-offset` in [`tests/docs/configuration.cpp`](../../tests/docs/configuration.cpp)
