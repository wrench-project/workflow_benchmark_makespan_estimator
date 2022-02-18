# Workflow analyzer

```
./workflow_benchmark_makespan_estimator --workflow ../data/blast-benchmark-200.json --flops_per_unit_of_cpu_work 100Gf --platform_spec summit --num_cores_per_node 10 --num_nodes 10 
```

# Estimates 

Running on n nodes, with p cores per noe

Naive no concurrency: sum of three times
  data to read   /  (n * read bw per node)
  data to write  /   (n * write bw per node)
  computation to do / (n * p * flop rate per core)

Naive full concurrency:  max of two times
    data to read /  (n * read bw per node) +  data to write / ( n *  write bw per node)
    computation to do / (n * p * flop rate per core)

Critical path approach:
    Find the critical path, duplicate tasks by a factor ceil(N/m) where N is the number
of tasks in the level and m is the total number of available cores
    Divide bw-per-node by number of cores-per-node
        

