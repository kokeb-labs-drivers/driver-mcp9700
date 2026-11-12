# Host stubs of the ESP-IDF headers

Just enough of ESP-IDF for `tests/esp32_port_check.cpp` to compile and run the port in `../main/` on a host
computer (CTest `mcp9700_esp32_port_check`). The stubs return success and a raw ADC count of 750; they model nothing
about the real ADC, so this guards the port's own logic only (which hooks it declares, what it does with and without
an enable pin), not its use of ESP-IDF.
