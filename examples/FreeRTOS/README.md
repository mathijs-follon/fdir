FreeRTOS integration example running on the POSIX/Linux simulator.

Three FreeRTOS tasks drive three scenarios sequentially:

1. Worker stops heartbeating - supervisor detects the watchdog miss and restarts it
2. Worker faults three times - restart budget exhausted, mode changes to DEGRADED
3. Two critical subsystems marked unavailable - mode changes to SAFE

Init the submodule before building either variant:

```
git submodule update --init examples/FreeRTOS/FreeRTOS-Kernel
```

## c/

Port hooks are strong overrides of the weak symbols in `port.c`, wired to `xQueueSend`, `xTaskGetTickCount`, and `vTaskSuspend`. `FreeRTOSConfig.h` is configured for the Posix port.

```
make freertos
./build/example_FreeRTOS
```

## cxx/

Port hooks are supplied as a `fdir::Port` struct of lambdas in `main.cpp`. The FreeRTOS kernel `.c` files are compiled as C; only the application file is compiled as C++. `FreeRTOSConfig.h` guards `vAssertCalled` with `extern "C"` when included from C++.

```
make freertos_cxx
./build/example_FreeRTOS_cxx
```
