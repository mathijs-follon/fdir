Minimal fdir example with no RTOS and no threads.

A single `main.c` registers one entity and drives it through three scenarios:

1. Single fault - fdir restarts the entity
2. Three faults - restart budget exhausted, mode changes to DEGRADED
3. Heartbeat miss - watchdog detects the stale entity and restarts it

Port hooks are implemented inline at the bottom of `main.c`. Copy and adapt them to your platform.

```
make getting_started
./build/getting_started
```
