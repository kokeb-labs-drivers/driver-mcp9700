# MCP970x ESP32 examples

ESP-IDF applications that run the MCP970x driver on an ESP32-C6, built by CI on every pull request. They are **not
verified on hardware**: the host build covers the driver logic, and the ADC full scale below is an estimate.

## Wiring (an ESP32-C6 board)

| ESP32-C6 | Sensor | Notes |
| --- | --- | --- |
| GPIO0 (ADC1 channel 0) | VOUT | 12 dB attenuation, curve-fitting calibration |
| 3V3 | VDD | 100 nF to GND at the pin |
| GND | GND | |

With this wiring the example uses `Esp32Mcp9700Port`, which has no `GpioSet()`: nothing is switched, `Sleep()` returns
`Unsupported` and `EnsureInitialized()` only brings the ADC up.

### Switched supply

To let the driver switch the sensor off, power VDD from a GPIO through 200 ohm with 1 uF at VDD (datasheet section
4.2), construct `Esp32Mcp9700SwitchedPort` with `Esp32Mcp9700Pins{.enable = <gpio>}` and give that type to
`mcp9700::MCP9700<>`. The switched port declares `GpioSet()`, so `Sleep()` cuts the power; its `enable` must be 0 or
more, and start-up fails with `Bus` if it is not (a port that declares the hook must be able to honour it).

## Build and flash

```bash
git submodule update --init scripts      # the ESP-IDF build scripts
. $IDF_PATH/export.sh                     # ESP-IDF v5.5
./scripts/build_app.sh list
./scripts/build_app.sh mcp9700_basic_example Debug
./scripts/flash_app.sh flash_monitor mcp9700_basic_example Debug
```

`idf.py menuconfig` shows "MCP970x analog temperature sensor" under Component config: the part, the High option,
the Custom coefficients, the ADC channel, the declared ADC full scale, the averaging and the calibration offset that
`mcp9700::KconfigDefaults()` returns. The example then sets the full scale and the resolution of its own port.

## The ADC full scale

`Esp32Mcp9700Port::kAdcFullScaleMv` is the largest calibrated millivolt value the port reports at 12 dB. It is
below the 3.3 V supply, because the calibrated range of a 12 dB channel ends near 3.1 V. The largest output of any
named part in its specified range is 2.84 V (MCP9701A at +125 C), so the clip check stays meaningful. The 3100 mV
is the example's estimate: measure the ceiling on your chip and set it before relying on the Saturated check.

## Expected output

```text
I (312) mcp9700_example: MCP970x driver 0.1.0
I (820) mcp9700_example: 23.456 C (VOUT 735 mV, spread 2 mV)
```

An `EnsureInitialized failed: DeviceIdMismatch` line means the start-up reading was not a plausible temperature (the
log line before it gives the reason): check VDD, the ADC channel and the wiring first.

## Files

| File | What it is |
| --- | --- |
| [`main/mcp9700_basic_example.cpp`](main/mcp9700_basic_example.cpp) | The example app |
| [`main/esp32_mcp9700_port.hpp`](main/esp32_mcp9700_port.hpp) | The ESP-IDF port: ADC, enable pin, delay, log |
| [`CMakeLists.txt`](CMakeLists.txt) | The ESP-IDF project; `EXTRA_COMPONENT_DIRS` points at the repository root, which is the component |
| [`app_config.yml`](app_config.yml) | The app matrix for the scripts, CI and the docs |
