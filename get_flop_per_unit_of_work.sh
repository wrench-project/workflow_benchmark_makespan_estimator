#!/bin/bash

g++ --std=c++17 src/cpu-benchmark.cpp -o /tmp/cpu-benchmark -lpthread
gcc src/linpack.c -o /tmp/linpack

CPU_BENCHMARK_WORK=1000

LINPACK_KFLOPS=`/tmp/linpack | grep 2048 | sed "s/.*%//" | sed "s/ *//"`
echo "LINPACK_KFLOPS = $LINPACK_KFLOPS"
CPU_BENCHMARK_TIME=`/tmp/cpu-benchmark $CPU_BENCHMARK_WORK 1 | grep TIME | sed "s/TIME: //"`
echo "BENCHMARK CPU_BENCHMARK_TIME = $CPU_BENCHMARK_TIME"

MFLOPS_PER_BENCHMARK_WORK_UNIT_EXPR="( $LINPACK_KFLOPS * $CPU_BENCHMARK_TIME ) / (1000.0 * $CPU_BENCHMARK_WORK )"


MFLOPS_PER_BENCHMARK_WORK_UNIT=`echo "$MFLOPS_PER_BENCHMARK_WORK_UNIT_EXPR" | bc -l`

echo "ONE CPU WORK UNIT ~= ${MFLOPS_PER_BENCHMARK_WORK_UNIT_EXPR} Mf"
echo "ONE CPU WORK UNIT ~= ${MFLOPS_PER_BENCHMARK_WORK_UNIT} Mf"



