FreeRTOS integration example running on the POSIX/Linux simulator.

Three FreeRTOS tasks drive three scenarios sequentially:

1. Worker stops heartbeating - supervisor detects the watchdog miss and restarts it
2. Worker faults three times - restart budget exhausted, mode changes to DEGRADED
3. Two critical subsystems marked unavailable - mode changes to SAFE

Port hooks (`port.c`) are wired to FreeRTOS queue and task primitives. `FreeRTOSConfig.h` is configured for the Posix port.

Init the submodule if not done yet:

```
git submodule update --init examples/FreeRTOS/FreeRTOS-Kernel
```

Build and run:

```
make freertos
./build/example_FreeRTOS
```
