#!/bin/bash

ASLFILE=$(basename -- ${1})
LLFILE=${ASLFILE/.asl/.ll}
rm -f ${LLFILE} a.out
./asl ${1} && clang -Wno-override-module ${LLFILE} && ./a.out < ${1/asl/in} | diff -y -  ${1/asl/out}
