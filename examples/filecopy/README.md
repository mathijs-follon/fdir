Parallel directory copy CLI supervised by fdir.

Each worker thread copies files from a shared job queue and heartbeats fdir after each file. On I/O failure fdir handles restart, budget exhaustion, and safe-mode escalation automatically. The process exits with a non-zero status if the system reached SAFE mode before all files were copied.

Port hooks are registered in `fdir_app_port()` and passed to `fdir_init()`. The job queue is a ring buffer guarded by a `pthread_mutex`. fdir's internal failure queue is drained by the supervisor thread via `fdir_supervisor_tick()`.

```
make filecopy
./build/fcopy <src> <dst> [--workers N]
```

`--workers` defaults to 4.
