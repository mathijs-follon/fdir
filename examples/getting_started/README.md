# getting_started

Minimal fdir example with no RTOS and no threads.

Registers one entity and drives it through three scenarios:

1. Single fault: fdir restarts the entity
2. Three faults: restart budget exhausted, mode changes to DEGRADED
3. Heartbeat miss: watchdog detects the stale entity and restarts it

Port hooks are registered in a `fdir_port_t` passed to `fdir_init()`.

```
make getting_started
./build/getting_started
```
