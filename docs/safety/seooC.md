# Software Element out of Context (SEooC)

This document states the assumptions and limits of the `fdir` library for
integrators building evidence under ECSS or IEC 61508. It is not a compliance
certificate.

## Purpose

`fdir` is a **supervisor framework** for embedded software: fault reporting,
bounded restart, heartbeat watchdog, subsystem/mode escalation, and event
notification. It is intended for CubeSat-scale and similar dependable embedded
systems.

## Assumed system context

| ID | Assumption |
|----|------------|
| A1 | A dedicated supervisor context calls `fdir_supervisor_tick()` periodically (see [threading.md](../threading.md)). |
| A1b | Ground may call `fdir_set_supervision_enabled(0)` to bypass FDIR; integrator accepts unsupervised operation while inhibited. |
| A2 | Fault **detection** (limits, plausibility, redundancy voting) is implemented by application code; the library detects heartbeat loss only. |
| A3 | **Isolation** is cooperative: workers use `fdir_worker_may_run()` and stop taking work when it returns zero. |
| A4 | **Recovery** (`restart` callbacks, reboot hook) is implemented by the integrator and verified at system level. |
| A5 | **Ground interface** (PUS, TM/TC, anomaly logging) is implemented in `emit_event`; this library does not format ECSS packets. |
| A6 | Entity/subsystem caps (`FDIR_ENTITY_CAP`, `FDIR_SUBSYSTEM_CAP`) and failure queue depth (`FDIR_FAILURE_QUEUE_CAP`, default 8) are sized for the mission and justified in the safety case. |

## Provided behaviour (element scope)

- Entity restart budgets (normal and watchdog paths) with configurable `on_exhausted` action.
- Internal bounded failure queue; supervisor drain via `fdir_supervisor_tick()`.
- Fault latch per entity+reason (ECSS 5.7.5.3c style deduplication of repeated reports).
- Events with stable `anomaly_id` (`FDIR_ANOMALY_ID`), `severity`, `phase` (raised/cleared), and `level` (entity/subsystem/system).
- Modes: NOMINAL, DEGRADED, SAFE, REBOOT_PENDING; ground de-escalation via `fdir_set_system_mode` / `fdir_deescalate_system_mode`.
- Optional subsystem handler `on_entity_exhausted` and system handler `on_system_escalation` in config.
- Critical fault flag `FDIR_FAULT_FLAG_CRITICAL`: `fdir_report_fault` returns `FDIR_ERR_BUSY` if the internal queue is full. `fdir_failure_queue_full_latched()` records overflow.

## Not provided (integrator responsibility)

- FMECA/FMEA traceability, anomaly catalogues, or telecommand inhibit of FDIR functions (ECSS 5.7.5.2k-n).
- Hardware redundancy switching, independent watchdog hardware, or F1/F2 timing proofs.
- Safe state definition for the spacecraft: `FDIR_MODE_SAFE` is a software mode flag only.
- Proof of diagnostic coverage or SIL (IEC 61508).
- Persistence of counters across power cycles (restart counts are RAM-only).

## Queue back-pressure

Failure reports are queued inside the library. Policy:

1. **Critical** reports (`FDIR_FAULT_FLAG_CRITICAL` or `fdir_report_fault`): never drop silently. `fdir_report_fault` returns `FDIR_ERR_BUSY` when full; entity health is unchanged so the caller can retry after the supervisor drains the queue. `fdir_failure_queue_full_latched()` is set.
2. **Non-critical** (`fdir_report_fault_ex` without critical flag): may return `FDIR_ERR_STATE` when full; document drop policy in the safety case.
3. The supervisor must call `fdir_supervisor_tick()` often enough to drain the queue under peak fault rates.
4. Log application queue saturation via `fdir_log_queue_overflow` where applicable.

## Hierarchical FDIR mapping

| ECSS level | `fdir` mechanism |
|------------|------------------|
| Entity | `fdir_entity_desc_t.decide`, restart budgets, `FDIR_LEVEL_ENTITY` events |
| Subsystem | `fdir_subsystem_desc_t.on_entity_exhausted`, subsystem marks, `FDIR_LEVEL_SUBSYSTEM` events on mark changes and policy overrides |
| System / ground | Mode escalation, `on_system_escalation`, `fdir_set_system_mode`, `FDIR_LEVEL_SYSTEM` events |

## Verification recommendations

- Unit tests in `tests/` cover latch, mode de-escalation, and recovery paths; extend for mission-specific `decide` / handlers.
- Integration tests: supervisor + worker concurrency with port `lock` enabled.
- System tests: demonstrate autonomous recovery and ground observability of `fdir_event_t` fields.

## Standards disclaimer

Use of this library **does not** constitute compliance with ECSS-E-ST-40C,
ECSS-Q-ST-80C, ECSS-E-ST-70-11, or IEC 61508. Compliance remains a property of
the **integrated system**, process, and verification evidence.
