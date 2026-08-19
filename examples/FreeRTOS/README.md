FreeRTOS integration example running on the POSIX/Linux simulator.

Three FreeRTOS tasks drive three scenarios sequentially:

1. Worker stops heartbeating: supervisor detects the watchdog miss and restarts it
2. Worker faults three times: restart budget exhausted, mode changes to DEGRADED
3. Two critical subsystems marked unavailable: mode changes to SAFE

Init the submodule before building:

```
git submodule update --init examples/FreeRTOS/FreeRTOS-Kernel
```

Port hooks are registered through `fdir_app_port()` and passed to `fdir_init()`. `FreeRTOSConfig.h` is configured for the Posix port.

```
make freertos
./build/example_FreeRTOS
```
