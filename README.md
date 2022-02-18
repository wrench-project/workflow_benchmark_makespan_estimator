# Workflow makespan estimator

```
./workflow_benchmark_makespan_estimator --help
```


```
./workflow_benchmark_makespan_estimator --workflow ../data/blast-benchmark-200.json --flops_per_unit_of_cpu_work 100Gf --platform_spec summit --num_cores_per_node 10 --num_nodes 10 
```

# Computed Estimates 

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
 

### Critical path appraoch

TBD
        

