Minimal fdir example with no RTOS and no threads.

Both variants register one entity and drive it through three scenarios:

1. Single fault - fdir restarts the entity
2. Three faults - restart budget exhausted, mode changes to DEGRADED
3. Heartbeat miss - watchdog detects the stale entity and restarts it

## c/

Port hooks are implemented as strong overrides of the weak symbols in `main.c`.

```
make getting_started
./build/getting_started
```

## cxx/

Port hooks are supplied as a `fdir::Port` struct of lambdas passed to `fdir::Supervisor::create()`. No global state is required for port configuration.

```
make getting_started_cxx
./build/getting_started_cxx
```
