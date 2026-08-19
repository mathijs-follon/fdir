Parallel directory copy CLI supervised by fdir.

Each worker thread copies files from a shared queue and heartbeats fdir after each file. On I/O failure fdir handles restart, budget exhaustion, and safe-mode escalation automatically.

```
make filecopy
./build/fcopy <src> <dst> [--workers N]
```

`--workers` defaults to 4. The process exits with a non-zero status if the system reached SAFE mode before all files were copied.
