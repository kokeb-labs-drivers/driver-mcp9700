# Datasheet map

The driver follows the MCP970X family datasheet (the MCP9700, MCP9700A, MCP9700B, MCP9701 and MCP9701A). This page
says which revision, where to get it, and where each part of
the driver is described in it, so you can check any behaviour against the source. Section names and page numbers
are the datasheet's own (the printed page numbers).


<!-- generated: datasheet-map -->
- **Datasheet: MCP970X Low-Power Linear Active Thermistor ICs**, Microchip Technology Inc. DS20001942, revision L
  (2024-02). Get it from the [vendor page](https://www.microchip.com/en-us/product/MCP9700) or as a
  [PDF](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf).
  SHA-256 of the file used: `5b0e0efe173d5fdaf72c31e697591b0b1cacbd440ec6ba95d742fd587c7aff33`.

| Topic | Datasheet section | Page | In the driver | Explained on |
| --- | --- | --- | --- | --- |
| Pins and packages | Package Types; 3.0 Pin Descriptions, Table 3-1 | [p. 2](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=2) | `CtrlPin::Enable`, `kPackageSot23`, `kPackageSc70`, `kPackageTo92` | [Hardware](hardware.md), [Quickstart](quickstart.md) |
| Absolute maximum ratings | 1.0 Electrical Characteristics | [p. 3](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=3) | `kMaxOutputCurrentUa`, `kMaxLoadCapacitancePf` (documented limits only: no register) | [Hardware](hardware.md), [Porting](porting.md) |
| Supply range and current | DC Electrical Characteristics | [p. 3](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=3) | `VariantSpec::vdd_min_mv`, `VariantSpec::vdd_max_mv`, `kVariantTable` | [Hardware](hardware.md), [Configuration](configuration.md) |
| Accuracy of each device | DC Electrical Characteristics, Sensor Accuracy | [p. 3](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=3) | `AccuracyLimitsFor()`, `AccuracyBand` | [Configuration](configuration.md), [Transfer function](transfer-function.md), [Limitations](limitations.md) |
| Output current, impedance and load capacitance | DC Electrical Characteristics (I_OUT, Z_OUT, C_LOAD) | [p. 4](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=4) | `kMaxOutputCurrentUa`, `kOutputImpedanceOhm`, `kMaxLoadCapacitancePf` | [Porting](porting.md), [Hardware](hardware.md) |
| Turn-on time | DC Electrical Characteristics (t_ON) | [p. 4](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=4) | `Config::power_on_settle_us`, `kPowerOnTypicalUs` | [Hardware](hardware.md), [Configuration](configuration.md) |
| Specified temperature ranges | Temperature Characteristics | [p. 4](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=4) | `TemperatureClass`, `VariantSpec::min_mc`, `VariantSpec::max_ext_mc`, `VariantSpec::max_high_mc` | [Configuration](configuration.md), [Hardware](hardware.md) |
| Thermal response | DC Electrical Characteristics (t_RES) | [p. 4](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=4) | `Config::samples`, `Config::sample_interval_us`, `kMaxAveragingWindowUs` | [Configuration](configuration.md), [Limitations](limitations.md) |
| Output voltage range and an output outside it | 3.2 Output Voltage Pin (VOUT) | [p. 9](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=9) | `Error::OutputLow`, `Error::OutputHigh`, `Config::range_margin_mc` | [Hardware](hardware.md), [Troubleshooting](troubleshooting.md), [Contract](contract.md), [Architecture](architecture.md) |
| Transfer function: V0 and TC of each device | 4.0 Applications Information, Equation 4-1; DC Electrical Characteristics | [p. 10](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=10) | `OutputMvToMilliC()`, `MilliCToOutputMv()`, `ToCelsius()`, `Variant` | [Transfer function](transfer-function.md), [Configuration](configuration.md) |
| Improving accuracy: a one-point calibration | 4.1 Improving Accuracy | [p. 10](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=10) | `Config::offset_cal_mc` | [Configuration](configuration.md) |
| Shutdown from an I/O pin | 4.2 Shutdown Using Microcontroller I/O Pin | [p. 11](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=11) | `CtrlPin::Enable`, `Sleep()`, `EnsureInitialized()` | [Porting](porting.md), [Hardware](hardware.md), [API guide](api-guide.md) |
| Layout and decoupling | 4.3 Layout Considerations | [p. 11](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=11) | `docs/hardware.yml` circuits | [Hardware](hardware.md) |
| Thermal considerations and self-heating | 4.4 Thermal Considerations | [p. 11](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=11) | documented limit only (no register) | [Hardware](hardware.md), [Limitations](limitations.md) |
| Packaging drawings and ordering codes | 5.0 Packaging Information; Product Identification System | [p. 13](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=13) | `docs/hardware.yml` package, variants | [Hardware](hardware.md) |
| The five devices and the packages each comes in | Product Identification System; 5.1 Package Marking | [p. 23](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf#page=23) | `Variant`, `VariantSpec::packages`, `VariantSpec::high_packages` | [Hardware](hardware.md), [MCP970x Driver](index.md), [Configuration](configuration.md) |
<!-- /generated -->

## Where the datasheet is silent or wrong

- **No block diagram.** The blocks on [Hardware](hardware.md#inside-the-chip) follow the description in section 4.0
  (p. 10) and are marked unverified.
- **Equation 4-2.** Note 3 on p. 3 cites "Equation 4-2" for the first-order equation; the transfer function is
  Equation 4-1 (p. 10), and 4-2 is the self-heating equation. The driver follows 4-1.
- **Turn-on time.** 800 µs typical, no maximum (p. 4): the wait is a knob ([Limitations](limitations.md)).
- **The High option.** The table rows for the High option cover -40 to +150 °C; the datasheet does not say that an H
  part also meets the Extended rows, so with `TemperatureClass::High` the driver uses the High rows alone.
- **Output load regulation.** The unit is missing in text extraction of p. 4; the page shows 2 Ω.

## When the datasheet and the driver differ

The driver is written against the revision above. If you have a newer revision, compare its revision history with
the topics above; a change to a pin, a register, a timing or a value the driver uses is a bug report for this
repository. Where the driver deliberately goes beyond the datasheet, the [Contract](contract.md) says so.

The repository never contains the PDF itself: vendors own their documents. Download it from the vendor, and compare
its SHA-256 with the one above to confirm you have the same file.
