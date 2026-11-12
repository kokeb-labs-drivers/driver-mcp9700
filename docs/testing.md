# Testing and CI

How the driver is checked: host tests against an independent model of the sensor and the ADC, a differential oracle,
a fuzzer that controls what the ADC returns, compiled documentation, an install test, ESP32 and Cortex-M builds,
static analysis and measured quality gates, on every pull request. Nothing here touches a real sensor: see
[Limitations](limitations.md) for what a bench still has to confirm.

## Host tests

```bash
cmake -S . -B build -DMCP9700_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The tests are written from the datasheet, not from the driver. [`tests/mock_mcp9700.hpp`](../tests/mock_mcp9700.hpp)
is a model of the sensor and the ADC in floating point with its own copy of the datasheet rows: the sensor (V0, TC,
accuracy offset, nonlinearity, line regulation, turn-on time, an unpowered level), the ADC (full scale, resolution,
offset and gain error, noise, clipping, channel-switch crosstalk, an external divider) and the faults (stuck low,
floating, spikes, a failing conversion). [`tests/reference_mcp9700.hpp`](../tests/reference_mcp9700.hpp) is a second,
plain implementation of the driver's rules in 128-bit rational arithmetic; the differential tests and the fuzzer
compare the driver with it.

| File | Covers |
| --- | --- |
| [`test_variant_table.cpp`](../tests/test_variant_table.cpp) | The table against the test's own copy of the datasheet, its invariants, the `Custom` rules at every boundary |
| [`test_conversion.cpp`](../tests/test_conversion.cpp) | `OutputMvToMilliC()` and its inverse at the datasheet reference points, rounding, against the oracle |
| [`test_accuracy.cpp`](../tests/test_accuracy.cpp) | `AccuracyLimitsFor()` for every band, its edges and errors |
| [`test_model.cpp`](../tests/test_model.cpp) | End to end over the model: every named variant and synthetic `Custom` parts, swept across temperature |
| [`test_validation.cpp`](../tests/test_validation.cpp) | `Validate()` one step inside and one outside every bound |
| [`test_errors.cpp`](../tests/test_errors.cpp) | The base `Error` values; every chip-specific value is returned; the never-returned values stay unreturned |
| [`test_lifecycle.cpp`](../tests/test_lifecycle.cpp) | Construct, start, stop, restart; no traffic before start; the supply pin levels |
| [`test_faults.cpp`](../tests/test_faults.cpp) | Each model fault maps to its error; bus failures; the precedence rule |
| [`test_timing.cpp`](../tests/test_timing.cpp) | The settle wait, the sample intervals and the exact number of conversions |
| [`test_calibration.cpp`](../tests/test_calibration.cpp) | The offset is added after the window check |
| [`test_boundaries.cpp`](../tests/test_boundaries.cpp) | The window edges to the millivolt, the saturation threshold, the spread limit, divider rounding, the `Custom` limits |
| [`test_raw.cpp`](../tests/test_raw.cpp) | Single-shot raw access and the last-conversion getter |
| [`test_kconfig.cpp`](../tests/test_kconfig.cpp) | `KconfigDefaults()` without Kconfig and with each variant's menuconfig values |
| [`test_version.cpp`](../tests/test_version.cpp) | The version getters follow the generated header |
| [`tests/fuzz/`](../tests/fuzz/fuzz_mcp9700.cpp) | The fuzz target; CTest replays its corpus (`mcp9700_fuzz_mcp9700_corpus`), see below |
| [`tests/compile_fail/`](../tests/compile_fail/port_without_delay.cpp) | Ports and configurations that must not compile |
| [`tests/docs/`](../tests/docs/quickstart.cpp) | Every code block on this site, compiled |
| [`tests/check_install.cmake`](../tests/check_install.cmake) | `find_package(mcp9700)` from an installed package |

The tests build with `-fno-exceptions -fno-rtti` and warnings as errors, on GCC and Clang. The host example runs as a
test too (`mcp9700_host_example`) and its output is part of the check.

## Fuzz tests

Everything the driver reads is a voltage from the port's ADC, so the reading is what a floating input, a short or a
wrong channel controls. [`tests/fuzz/fuzz_mcp9700.cpp`](../tests/fuzz/fuzz_mcp9700.cpp) lets libFuzzer choose a
configuration (sometimes invalid: any variant including `Custom` with random fields, a named variant with a stray
`custom` field, a variant outside the enumeration), then up to 64 driver calls, and on every conversion what the
ADC does: pass the model's output, fail, or return any 32-bit value. Between calls it can change the temperature,
open the output, rewire the channel or power the sensor down. The port records everything the driver did in the last
call, and after every call the target checks these promises against the oracle and aborts, naming the line, when
one breaks:

| ID | Promise |
| --- | --- |
| F1 | A failed call returns an error this driver may return (never `Timeout`, `Crc`, `Busy` or `DeviceFault`) and a zero value |
| F2 | Before a successful start, and after `Deinitialize()` or `Sleep()`, every call but `EnsureInitialized()` and `Deinitialize()` is `NotInitialized` with zero traffic |
| F3 | `InvalidArgument` means zero traffic, and happens only when the oracle rejects the configuration |
| F4 | A successful reading used exactly `discard + samples` conversions and the right waits |
| F5 | The error and the value equal the oracle's, recomputed from the recorded conversions |
| F6 | A success holds no conversion near the full scale and no spread above the limit: nothing out of range is ever `Ok` |
| F7 | `ReadRawPortMv()` sends one conversion and returns what the port gave; the last-value getter follows every conversion |
| F8 | `Bus` exactly when a conversion, the ADC bring-up or the supply write failed in that call |
| F9 | After a successful stop the supply was driven INACTIVE; a failed start switches it off; only `EnsureInitialized()` ever drives it ACTIVE |
| F10 | A failed start leaves the driver uninitialised; a second start after success is free |
| F11 | The waits of a successful start total at least the settle time, and none is longer than the longest knob |
| F12 | Every log line is non-empty plain text |
| F13 | No undefined behaviour for any input (UBSan with `-fno-sanitize-recover=all`) |
| F14 | The named-variant, `Custom` and `High` rules agree with the oracle |
| F15 | A `Custom` start logs its one warning |

```bash
# libFuzzer needs Clang (on macOS, Homebrew's LLVM)
cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DMCP9700_BUILD_TESTS=ON -DMCP9700_BUILD_FUZZERS=ON
cmake --build build-fuzz --target fuzz_mcp9700
./build-fuzz/tests/fuzz/fuzz_mcp9700 -max_total_time=120 tests/fuzz/corpus/fuzz_mcp9700
```

The same file also builds without libFuzzer as `fuzz_replay_fuzz_mcp9700`, and CTest replays every input in
[`tests/fuzz/corpus/fuzz_mcp9700/`](../tests/fuzz/corpus/fuzz_mcp9700) on GCC and Clang
(`mcp9700_fuzz_mcp9700_corpus`): the seeds (`custom-valid`, `custom-invalid`, `named-with-custom`, `not-started`,
`init-and-read`, `bus-faults`, `config-invalid`, `stuck-low`, `float-high`, `saturated`, `unstable`, `power-cycle`,
`mcp9701-high-temperature`, `max-samples-max-divider`, `raw-extremes`) and every input that ever broke a promise (name
it `regress-*`), so a fixed bug stays fixed. CI fuzzes for 120 s on every pull request, from the seeds plus a corpus it
keeps between runs (job `Host quality / fuzz`).

## Quality gates

The `Host quality` job measures the library code, `inc/` and `src/` only (tests and examples are left out), and
fails when a value is over the limits of this repository's assurance level, `high` (set in
[`.kokeb/handbook.yml`](../.kokeb/handbook.yml), defined by the handbook's ADR-0020):

| Gate | Tool and how it is measured | Limit at `high` |
| --- | --- | --- |
| Complexity | lizard 1.24.1, modified cyclomatic complexity (`-m`: a `switch` counts once) per function | CCN at most 15 |
| Function length | lizard, lines of code per function | at most 60 |
| Parameters | lizard, per function | at most 6 |
| Line coverage | GCC 14 `--coverage -O0`, every CTest, gcovr 8.7 `--merge-lines` (a template's lines count once across its instances), `--exclude-unreachable-branches --exclude-throw-branches` | at least 85 % |
| Branch coverage | same run | at least 75 % |
| MC/DC | GCC `-fcondition-coverage`, same run | measured, not gated at `high` (100 % at `safety`) |
| Sanitizers | Clang, `-fsanitize=address,undefined -fno-sanitize-recover=all`, every CTest | no report |
| Fuzzing | libFuzzer with ASan and UBSan, 120 s per target | no broken promise |

Measured for this driver (the gate output prints all three): lines 100 % (328 of 328), branches 99 % (226 of 227),
conditions 99 % (209 of 210).

The reports are in the run's `host-quality-reports` artifact and its step summary. To run the gates locally:

```bash
# Coverage (GCC 14 or later), then the complexity limits
cmake -S . -B build-cov -DCMAKE_CXX_COMPILER=g++-14 -DMCP9700_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="--coverage -O0 -fcondition-coverage"
cmake --build build-cov && ctest --test-dir build-cov
gcovr --root . --filter inc/ --filter src/ --merge-lines --exclude-unreachable-branches --txt build-cov
lizard -m -C 15 -L 60 -a 6 inc src

# Sanitizers (Clang)
cmake -S . -B build-san -DCMAKE_CXX_COMPILER=clang++ -DMCP9700_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -O1 -g"
cmake --build build-san && ctest --test-dir build-san --output-on-failure
```

The fuzz gate runs locally as shown in [Fuzz tests](#fuzz-tests).

clang-tidy 20 ([`_config/.clang-tidy`](../_config/.clang-tidy): bugprone, CERT, clang-analyzer,
cppcoreguidelines, modernize, performance, portability and readability, each disabled check with its reason) and
cppcheck (strict) run in the `C++ lint` and `Repository checks` jobs and fail on any finding.

The `MISRA` job ([`ci-misra.yml`](../.github/workflows/ci-misra.yml)) runs the shared `ru-misra` workflow of
ci-tools at level `high`: it traces a host build and the Cortex-M build (`mcp9700_optimised_build`) of every
translation unit, checks them against MISRA C++:2023 and the handbook's C++20 profile (the CodeQL coding-standards
and profile packs, plus the validated clang-tidy and cppcheck checks), and reports each finding against the
handbook's compliance matrix and deviation records. A finding is fixed in the code; a deliberate exception names
its deviation on the line before it (`// codeql::misra_deviation_next_line(MD-CPP-009)` above the `.ipp` include
of `inc/mcp9700.hpp`). Until the repository is public the job runs with `open_source_prerelease: true`, the
declaration that the code is released under the licence in `LICENSE` and is to be published.

## CI workflows

Everything runs from [`ci.yml`](../.github/workflows/ci.yml), which calls one workflow file per check and ends in
the `CI gate` job, the single required check (`CI gate / gate`).

| Job | What it checks |
| --- | --- |
| Host tests | GCC, then Clang (one job), build and run every test above, then build and run `examples/host` on its own |
| Host quality | The gates above: complexity, coverage, sanitizers, fuzzing |
| Cross-compile | `arm-none-eabi-g++` for Cortex-M0+, M4F and M7F with the strict flags, warnings as errors (nothing linked) |
| C++ lint | clang-format and clang-tidy over the sources |
| MISRA | MISRA C++:2023 and the C++20 profile on traced host and Cortex-M builds, at level `high` (see [Quality gates](#quality-gates)) |
| Docs | Strict site build (`_config/mkdocs.yml`), generated-region drift, Doxygen (`_config/Doxyfile`), offline link check |
| Repository checks | One job: DCO on pull requests, the org standard, Markdown and YAML lint, the [Kokeb Labs handbook](https://github.com/kokeb-labs/handbook) rules for `.kokeb/handbook.yml` (by rule ID), template drift (warns only) and cppcheck over the driver sources |
| CI gate | Passes only if every job above passed: the one required check |

The **ESP32 examples** job builds every app in `examples/esp32/app_config.yml` with the repository root as the
driver's component. The gate needs it too.
