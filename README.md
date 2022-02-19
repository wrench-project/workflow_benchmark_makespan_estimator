# Workflow makespan estimator

```
./workflow_benchmark_makespan_estimator --help
```


```
./workflow_benchmark_makespan_estimator --workflow ../data/blast-benchmark-200.json --flops_per_unit_of_cpu_work 100Gf --platform_spec summit --num_cores_per_node 10 --num_nodes 10 
```

Script to compute the value to pass as a value to the `--flops_per_unit_of_cpu_work` command-line option of the estimator:

```
./get_flop_per_unit_of_work.sh
```

Here are invocations of this script:

dirt02:
```
LINPACK_KFLOPS = 378025.426
BENCHMARK CPU_BENCHMARK_TIME = 46.4698
ONE CPU WORK UNIT ~= ( 378025.426 * 46.4698 ) / (1000.0 * 1000 ) Mf
ONE CPU WORK UNIT ~= 17.56676594113480000000Mf
```

Henri's laptop:
```
LINPACK_KFLOPS = 1163460.991
BENCHMARK CPU_BENCHMARK_TIME = 18.7519
ONE CPU WORK UNIT ~= ( 1163460.991 * 18.7519 ) / (1000.0 * 1000 ) Mf
ONE CPU WORK UNIT ~= 21.81710415713290000000Mf
```

Summit: 
```
LINPACK_KFLOPS = 358855.910
BENCHMARK CPU_BENCHMARK_TIME = 41.0966
ONE CPU WORK UNIT ~= ( 358855.910 * 41.0966 ) / (1000.0 * 1000 ) Mf
ONE CPU WORK UNIT ~= 14.74775779090600000000 Mf
```





# Computed Estimates 

### Platform specification

Running on n p-core nodes with:

  - floprate: A known per-core flop/sec rate
  - rbandwidth: A known per-node I/O read bandwidth
  - wbandwidth: A known per-node I/O write bandwidth

### Naive, no-concurrency estimate

  - rdata: total data amount read by the workflow
  - wdata: total data amount written by the workflow
  - flops: total flops computed by the workflow

The estimate is computed as: rdata / (n * rbandwidth) + flops / (n * p * floprate) + wdata / (n * wbandwidth)
  
### Naive, concurrency estimate

  - rdata: total data amount read by the workflow
  - wdata: total data amount written by the workflow
  - flops: total flops computed by the workflow

The estimate is computed as: max(rdata / (n * rbandwidth)  + wdata / (n * wbandwidth), flops / (n * p * floprate))
 
### Critical path approach

This estimate is computed using a level-by-level model of the execution. For
each level, a makespan is computed, and the overall estimate is the sum of
these makespans. The makespan for each level is compute as follows.

The way in which the execution of a level proceeds is highly dependent on
the scheduling algorithm used to allocate tasks to cores, so we're only
after a reasonable estimate.  We model a level's execution consists of
multiple phases in which each phase executes up to n * p tasks. So given a
level with N tasks, the number of phases is ceil(N / (n * p)). The tasks
are sorted by their I/O times + compute times assuming they run alone on
the machine on a single core. In each phase, for each task in that phase,
we compute their execution times accounting for I/O contention, and then
say that the phase runs in time equal to the average task execution time.
The goal is to have a broad approximation of task execution overlaps
between different phases of the workflow.




