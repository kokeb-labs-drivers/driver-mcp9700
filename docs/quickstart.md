# Quickstart

Read the temperature from your MCU: wire three signals, write a small port class, call two driver functions.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Microchip Technology Inc. DS20001942 L:

    - Pins and packages: Package Types; 3.0 Pin Descriptions, Table 3-1, [p. 2](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=2)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## 1. Wire it

| MCU | Sensor | Notes |
| --- | --- | --- |
| ADC input | VOUT | The input must reach at least 2 V for an MCP9700 (1.75 V at +125 °C) |
| 3.3 V | VDD | 100 nF to GND close to the pin; an MCP9701 or MCP9701A needs 3.1 V or more |
| GND | GND | Same ground as the ADC reference |

The pin numbers depend on the package; [Hardware](hardware.md) has all three. Do not put a resistor in series
with VOUT. If you want the driver to switch the sensor off, power VDD from an MCU pin through 200 Ω instead (see
[Hardware](hardware.md#power-from-an-io-pin)).

## 2. Add the driver

```bash
git submodule add https://github.com/kokeb-labs-drivers/driver-mcp9700.git external/driver-mcp9700
```

## 3. Write the port

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

The port returns millivolts at the ADC pin, calibrated. It does not return raw counts: the driver needs a
voltage, and a count scaled by a guess is not one.

## 4. Read the temperature

```cpp title="main.cpp"
--8<-- "tests/docs/quickstart.cpp:quickstart"
```

1. `EnsureInitialized()` checks the configuration, brings the ADC up, waits for the sensor to turn on and takes one
   validated reading, a weak analogue of a chip-ID check (it rules out a dead or grounded line, not a floating one).
2. `ReadTemperatureMilliC()` discards one dummy conversion, averages eight, checks them for a clipped ADC, noise and
   an output outside the specified range, and applies the transfer function. 25000 is 25.000 °C.

The defaults describe an MCP9700 behind a 12-bit ADC with a 3.3 V full scale. For another part or ADC, set the
variant and the ADC's full scale and resolution in `Config` ([Configuration](configuration.md)): a wrong full scale
hides a clipped reading.

Next: [API guide](api-guide.md).
