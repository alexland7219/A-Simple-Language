# A Simple Language
This is a compiler for ASL (A Simple Language) written in C++, main project of Compilers (FIB-UPC) 2023.

## Prerequisits

Have ANTLRv4 installed on your GNU/Linux system on `/usr/local` (otherwise the Makefile won't compile the compiler). 

## How to compile the compiler

`make antlr` first, then `make` on the `asl` directory. This will compile a binary file `./aslasl` that compiles ASL.

## How to execute?

The compiler translates ASL code into TVM code. In order to execute the TVM code you can use the `tvm` executable provided in this repo.

## ASL: Syntax and Semantics

Please refer to http://web.archive.org/web/20230608120358/https://www.cs.upc.edu/~cl/practica/asl.html
