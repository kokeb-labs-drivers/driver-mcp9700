---
hide:
  - navigation
  - toc
---

<div class="kb-hero" markdown>
<span class="kb-kicker">Driver</span>

# MCP970x Driver

<p>A C++20 driver for the Microchip MCP9700, MCP9700A, MCP9700B, MCP9701 and MCP9701A analog temperature sensors,
read through any MCU's ADC, with clip, noise and range checks.</p>

[Quickstart](quickstart.md){ .md-button .md-button--primary }
[API guide](api-guide.md){ .md-button }
[Source on GitHub](https://github.com/kokeb-labs-drivers/driver-mcp9700){ .md-button }
</div>

The driver has no platform code. You give it a small class that reads one ADC channel in millivolts; it does the
rest: power-up timing, averaging, the clip, noise and range checks, and the transfer function in integers. Other
linear active thermistor ICs with a first-order output (V = V0 + TC x T) work through an explicit `Custom` variant
that you declare completely.

<div class="kb-chip" markdown>
<div class="kb-chip__head">
<span class="kb-chip__kicker">Analog temperature sensor</span>
<span class="kb-chip__name">MCP9700 / MCP9700A / MCP9700B / MCP9701 / MCP9701A</span>
<span class="kb-chip__vendor">Microchip</span>
</div>

Measures
:   -40 °C to +125 °C (+150 °C with the High option on the MCP9700 and MCP9700B); -10 °C to +125 °C for the
    MCP9701 and MCP9701A

Output
:   500 mV at 0 °C and 10.0 mV/°C (MCP9701 and MCP9701A: 400 mV and 19.5 mV/°C), typical

Accuracy
:   ±4 °C (MCP9700, MCP9701), ±2 °C (A grade), ±1 °C (MCP9700B, +20 °C to +70 °C), by temperature span

Supply
:   2.3 V to 5.5 V (3.1 V to 5.5 V for the MCP9701 and MCP9701A), 6 µA typical

Interface
:   One analog output (VOUT) to an MCU ADC channel; the supply may come from an MCU pin (optional hook)
</div>

## What you get

- **One object per sensor**, no I/O until `EnsureInitialized()`. See [API guide](api-guide.md).
- **The whole family as data**: one variant table, no branch on a part name, plus the explicit `Custom` variant.
  See [Configuration](configuration.md).
- **Integers only**: milli-degrees and millivolts, with one optional `ToCelsius()` for printing. See
  [Transfer function](transfer-function.md).
- **A reading you can trust**: dummy conversions, averaging, clip (`Saturated`), noise (`Unstable`) and range
  (`OutputLow`, `OutputHigh`) checks, never a retry and never a clamp. See [Architecture](architecture.md).
- **Errors you can act on**: `Result<T>` with an `Error`. See [Contract](contract.md).
- **Zero-overhead portability**: a CRTP port checked by a concept, one required hook for the ADC. See
  [Porting](porting.md).
- **Tested without hardware** against an independent model of the sensor and the ADC, and fuzzed. See
  [Testing and CI](testing.md). What a bench still has to confirm is in [Limitations](limitations.md).

## Datasheet

<!-- generated: datasheet-card -->
| Document | Revision | Get it |
| --- | --- | --- |
| Datasheet: MCP970X Low-Power Linear Active Thermistor ICs (Microchip Technology Inc. DS20001942) | L, 2024-02 | [PDF](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP970X-Family-Data-Sheet-DS20001942.pdf) · [vendor page](https://www.microchip.com/en-us/product/MCP9700) |

Written against these revisions; the SHA-256 of each file is in [`docs/datasheets.yml`](datasheets.yml). Where each part
of the driver is in the datasheet: [Datasheet map](datasheet.md).
<!-- /generated -->
