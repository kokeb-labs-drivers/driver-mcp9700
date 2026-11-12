# Examples

One folder per platform under `examples/`, each with its own port, built by CI on every pull request. The driver
itself has no platform code; [Platforms](platforms.md) lists what exists.

## Host

[`examples/host/`](../examples/host/README.md) runs the driver on any PC against a simulated sensor and ADC, through the
host port [`host_mcp9700_port.hpp`](../examples/host/host_mcp9700_port.hpp). No hardware:

```bash
cmake -S examples/host -B build-example && cmake --build build-example && ./build-example/mcp9700_host_example
```

It starts the sensor, reads 25, 85 and -10 °C, calibrates a part that reads 3 °C high, pulls the sensor out
(`OutputLow`), puts it to sleep and wakes it. The host tests build it with warnings as errors and run it, and its
output is part of the check (CTest `mcp9700_host_example`).

## ESP32

ESP-IDF applications that run the driver on an ESP32-C6 board (ADC1 channel 0 on GPIO0, 12 dB, curve-fitting
calibration); not verified on hardware. The wiring is in the
[example folder's README](../examples/esp32/README.md) and the port in
[`esp32_mcp9700_port.hpp`](../examples/esp32/main/esp32_mcp9700_port.hpp), which has one port for a sensor wired to
the supply and one that also drives a power pin. The project points
`EXTRA_COMPONENT_DIRS` at the repository root, which is the driver's ESP-IDF component.

### The apps

<!-- generated: examples -->
| App | What it shows | Source | CI |
| --- | --- | --- | --- |
| `mcp9700_basic_example` :material-star:{ title="featured" } | Initialise the sensor from menuconfig values and log the temperature twice a second | [`mcp9700_basic_example.cpp`](../examples/esp32/main/mcp9700_basic_example.cpp) | yes |

Build and flash one (from `examples/esp32`, ESP-IDF environment loaded):

```bash
./scripts/build_app.sh mcp9700_basic_example Release
./scripts/flash_app.sh flash_monitor mcp9700_basic_example Release
```

Source of this table: [`examples/esp32/app_config.yml`](../examples/esp32/app_config.yml).
<!-- /generated -->

### Set up once

```bash
git submodule update --init examples/esp32/scripts   # the ESP-IDF build scripts
cd examples/esp32
. $IDF_PATH/export.sh                                 # ESP-IDF v5.5
./scripts/build_app.sh list                           # every app and build type
```

The scripts read [`app_config.yml`](../examples/esp32/app_config.yml) (the same file this page's table comes
from) and build each app as its own firmware image.

### What to read in an example

| You want to | Read |
| --- | --- |
| See the lifecycle and every `Result` checked | `mcp9700_basic_example.cpp` |
| Declare the port's full scale and resolution | `esp32_mcp9700_port.hpp` and `mcp9700_basic_example.cpp` |
| Write an ESP-IDF port | `esp32_mcp9700_port.hpp` |
| Use menuconfig values | `mcp9700_kconfig.hpp` and [`_config/Kconfig`](../_config/Kconfig) |
