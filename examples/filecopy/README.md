Parallel directory copy CLI supervised by fdir.

Each worker thread copies files from a shared job queue and heartbeats fdir after each file. On I/O failure fdir handles restart, budget exhaustion, and safe-mode escalation automatically. The process exits with a non-zero status if the system reached SAFE mode before all files were copied.

## c/

Port hooks are strong overrides of the weak symbols. The failure queue is a ring buffer guarded by a `pthread_mutex`.

```
make filecopy
./build/fcopy <src> <dst> [--workers N]
```

## cxx/

Port hooks are supplied as a `fdir::Port` struct of lambdas. The job queue is a `JobQueue` class using `std::mutex` and `std::condition_variable`. Workers are `std::thread` instances stored in `WorkerCtx`.

```
make filecopy_cxx
./build/fcopy_cxx <src> <dst> [--workers N]
```

`--workers` defaults to 4 in both variants.
