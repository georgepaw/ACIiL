LLVM_BIN_ROOT ?= /Users/george/llvm6/llvm-dbg/bin/

CC  = $(LLVM_BIN_ROOT)clang
CXX = $(LLVM_BIN_ROOT)clang++

CFLAGS=-flto -O2
ACIIL_LIB_PATH=$(shell pwd)/../lib
LDFLAGS=-lm -v -mllvm -load -mllvm $(ACIIL_LIB_PATH)/libLLVMACIiL.so

.PHONY: cr clean

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

simple: simple.o

simple2: simple2.o

%.bc: %.c
	$(CC) -O3 -c -emit-llvm $<

cr: checkpoint.bc restart.bc

clean:
	rm *.bc simple.o simple.c
