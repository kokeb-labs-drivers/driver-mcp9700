# MCP970x host example

The driver on your PC, with no hardware: the host port in
[`host_mcp9700_port.hpp`](host_mcp9700_port.hpp) connects the real driver to a simulated sensor and ADC (the model
the host tests use), and [`main.cpp`](main.cpp) starts it, reads 25, 85 and -10 C, calibrates a part that reads 3 C
high, pulls the sensor out, and puts it to sleep and wakes it.

## Build and run

From the repository root, with CMake 3.16+ and a C++20 compiler:

```bash
cmake -S examples/host -B build-example
cmake --build build-example
./build-example/mcp9700_host_example
```

## Expected output

```text
MCP970x driver 0.1.0 on the host
one ADC step is 81 mC
true 25 C: VOUT 750 mV, spread 0 mV, reads 25.000 C
true 85 C: VOUT 1350 mV, spread 0 mV, reads 85.000 C
true -10 C: VOUT 400 mV, spread 0 mV, reads -10.000 C
as float: -10.0 C
part reads 28.000 C
calibrated reads 25.000 C
pulled out: OutputLow
asleep: NotInitialized
woken, reads 25.000 C
```

The host tests build this folder too and run the example (CTest `mcp9700_host_example`), so CI fails if it stops
compiling or a call fails.

## Files

| File | What it is |
| --- | --- |
| [`main.cpp`](main.cpp) | The example: start, read, calibrate, fault, sleep |
| [`host_mcp9700_port.hpp`](host_mcp9700_port.hpp) | The host port: the ADC hook over the simulated sensor, `GpioSet()`, `DelayUs()` and `Log()` |
| [`CMakeLists.txt`](CMakeLists.txt) | Standalone project; adds the driver with `add_subdirectory(../..)` |
