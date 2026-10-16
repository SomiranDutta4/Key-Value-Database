## Performance Benchmarks

The following benchmarks were performed on the current ForgeDB prototype.

> These results are intended as experimental measurements for the current implementation and development environment. ForgeDB is a learning-focused storage engine prototype and is not intended to provide production-grade benchmark guarantees.

### Write Benchmark

| Metric | Result |
|---|---:|
| Operations | 10,000 |
| Total Time | 0.01 seconds |
| Throughput | 782,110 writes/second |
| Average Latency | 1.28 microseconds/write |

### Read Benchmark

| Metric | Result |
|---|---:|
| Operations | 10,000 |
| Total Time | 0.01 seconds |
| Throughput | 1,868,271 reads/second |
| Average Latency | 0.54 microseconds/read |

### SYNC vs ASYNC Durability

| Mode | Throughput | Average Latency |
|---|---:|---:|
| ASYNC | 781,953 writes/second | 1.28 microseconds/write |
| SYNC | 255,497 writes/second | 3.91 microseconds/write |

**ASYNC / SYNC throughput ratio: 3.06x**

These results demonstrate the expected durability trade-off:

- **ASYNC mode** provides higher write throughput because writes are not forced to disk after every operation.
- **SYNC mode** provides stronger durability guarantees by synchronizing writes more aggressively, at the cost of lower throughput and higher latency.

### Benchmark Scope

The benchmarks exercise the current ForgeDB implementation, including the database command path and the configured durability modes. Results can vary depending on hardware, operating system, filesystem behavior, compiler optimizations, workload size, and database state.

The benchmarks should therefore be interpreted as measurements of the current prototype rather than universal performance claims.
