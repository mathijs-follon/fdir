# dual_path

Dual critical-path FDIR example for satellite-style systems. No RTOS required.

Demonstrates:

- Two entities (downlink, storage) with **per-entity `decide()` policy**
- Critical subsystems and dual-path SAFE escalation
- **`fdir_worker_may_run()`** in worker loops (SAFE-mode heartbeat built in)
- **`fdir_supervisor_tick()`** as the supervisor entry point

```
make dual_path
./build/dual_path
```

See [docs/integration.md](../../docs/integration.md).
