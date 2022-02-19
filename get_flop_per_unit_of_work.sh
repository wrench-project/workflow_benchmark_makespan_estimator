#!/bin/bash

g++ --std=c++17 src/cpu-benchmark.cpp -o /tmp/cpu-benchmark
gcc src/linpack.c -o /tmp/linpack

CPU_BENCHMARK_WORK=100

LINPACK_KFLOPS=`/tmp/linpack | grep 2048 | sed "s/.*%//" | sed "s/ *//"`
echo "LINPACK_KFLOPS = $LINPACK_KFLOPS"
CPU_BENCHMARK_TIME=`/tmp/cpu-benchmark $CPU_BENCHMARK_WORK 1 | grep TIME | sed "s/TIME: //"`
echo "BENCHMARK CPU_BENCHMARK_TIME = $CPU_BENCHMARK_TIME"

MFLOPS_PER_BENCHMARK_WORK_UNIT=`echo "( $LINPACK_KFLOPS * $CPU_BENCHMARK_TIME ) / (1000.0 * $CPU_BENCHMARK_WORK )" | bc -l`

echo "ONE CPU WORK UNIT ~= ${MFLOPS_PER_BENCHMARK_WORK_UNIT}Mf"



