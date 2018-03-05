LLVM_BIN_ROOT ?= ../../llvm-dbg/bin/

CC  = $(LLVM_BIN_ROOT)clang
CXX = $(LLVM_BIN_ROOT)clang++

CFLAGS=-flto -O3

LDFLAGS= -lm -lstdc++

.PHONY: cr clean

%.o: %.c cr
	$(CC) $(CFLAGS) -o $@ -c $<

simple: simple.o

simple2: simple2.o

vecadd: vecadd.o

vecadd_ptrswp: vecadd_ptrswp.o

jacobi: jacobi.o

jacobi-malloc: jacobi-malloc.o

jacobi-global-vars: jacobi-global-vars.o

test: test.o

%.bc: %.cpp
	$(CXX) -std=c++11 -O3 -c -emit-llvm $<

cr: checkpoint.bc restart.bc ACRIiLState.bc

clean:
	rm -rf .acriil_chkpnt-*
