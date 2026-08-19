# fdir

[![Compile and Test](https://github.com/mathijs-follon/fdir/actions/workflows/compile-and-test.yml/badge.svg)](https://github.com/mathijs-follon/fdir/actions/workflows/compile-and-test.yml)

Portable C11 FDIR (Fault Detection, Isolation and Recovery) library for embedded and RTOS applications.

FDIR is a fault-management pattern used in spacecraft, aviation, and safety-critical embedded systems. When a software component fails, the system detects the fault, isolates the affected entity, and applies a configurable recovery action: restart, degradation, safe mode, or reboot. `fdir` provides a small, deterministic framework for this behaviour.

The core library uses no dynamic allocation and has no operating-system dependency. Platform-specific behaviour is provided through a small set of port hooks. A C++20/23 header-only interface is also provided.

## Design

The following properties are addressed by the design:

- explicit fault detection and fault reporting
- separation between fault detection and recovery decisions
- deterministic recovery policies
- watchdog and heartbeat supervision
- bounded restart attempts
- explicit degraded and safe operating modes
- isolation of affected software entities
- subsystem-level escalation
- explicit platform integration points
- no dynamic memory allocation in the core
- testable recovery behaviour
- portability across embedded and RTOS environments

## Standards awareness

`fdir` is developed with safety- and mission-critical engineering practices in mind. The design addresses engineering concerns commonly encountered in dependable embedded and space software.

The project does not claim compliance or certification against any standard such as ECSS-E-ST-40C, ECSS-Q-ST-80C, or IEC 61508. Compliance is a property of the complete system, development process, configuration, and evidence -- not of this library in isolation. The integrating project is responsible for determining which standards apply and for producing the required assurance evidence.

Whether these properties contribute to compliance with a particular standard depends on the applicable requirements, project tailoring, implementation, and verification evidence.

The project may be used as a software component within systems developed under standards such as ECSS-E-ST-40C, ECSS-Q-ST-80C, or IEC 61508, but using it does not by itself constitute compliance with those standards.

## Concepts

**Entity:** a task, thread, or software component registered with the framework. Each entity has a restart budget and a watchdog heartbeat. When a fault is reported fdir selects a recovery action (restart, degrade, safe mode, reboot) based on the entity's descriptor and the current system state.

**Subsystem:** a logical grouping of entities that can be marked available, degraded, or unavailable. Critical-path subsystems participate in the safe-mode threshold check.

**Mode:** the global operating mode, driven only by the supervisor recovery path:

| Mode | Value | Meaning |
|------|-------|---------|
| NOMINAL | 0 | All entities within budget |
| DEGRADED | 1 | At least one entity exhausted its restart budget |
| SAFE | 2 | Too many critical subsystems unavailable |
| REBOOT_PENDING | 3 | Reboot has been requested |

## Building

```
make          # build libfdir.a and compile_commands.json
make test     # build and run all tests
make examples # build all six example binaries
make clean
```

## Examples

Each example has a `c/` and a `cxx/` subdirectory. The C variants use strong overrides of the weak port-hook symbols. The C++ variants supply port hooks as a `fdir::Port` struct of lambdas passed to `fdir::Supervisor::create()`.

### getting_started

Minimal example with no RTOS and no threads. Covers fault/restart, budget exhaustion, and watchdog detection.

```
make getting_started        # c/
make getting_started_cxx    # cxx/
```

### filecopy

Parallel directory copy CLI. Worker threads share a bounded job queue and heartbeat fdir after each file. Demonstrates multi-entity registration and fault-driven restart/degrade on I/O errors.

```
make filecopy               # c/
./build/fcopy <src> <dst> [--workers N]

make filecopy_cxx           # cxx/
./build/fcopy_cxx <src> <dst> [--workers N]
```

### FreeRTOS

FreeRTOS integration on the POSIX/Linux simulator. Three tasks (worker, supervisor, scenario driver) drive watchdog miss, budget exhaustion, and dual-path SAFE mode.

```
git submodule update --init examples/FreeRTOS/FreeRTOS-Kernel

make freertos               # c/
./build/example_FreeRTOS

make freertos_cxx           # cxx/ (kernel compiled as C, application as C++)
./build/example_FreeRTOS_cxx
```

## Questions and support

If you have questions about integrating the library, run into unexpected behaviour, or want to discuss how to apply it to your system, open an issue on GitHub. I am happy to help.

## API reference

Full API reference including integration examples, port hook documentation, and C++ types: [docs/api.md](docs/api.md).
