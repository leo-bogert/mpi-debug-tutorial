#!/bin/bash
set -o errexit

module purge
module add gcc/4.7.0
module add mpi/openmpi/1.6.1/gcc_4.7.0

mpicc matrix-multiply.c -std=c99 -o matrix-multiply
mpicc matrix-multiply-simple-debug.c -std=c99 -o matrix-multiply-simple-debug

