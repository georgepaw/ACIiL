LLVM_BIN_ROOT ?= ../../llvm-dbg/bin/

CC  = $(LLVM_BIN_ROOT)clang
CXX = $(LLVM_BIN_ROOT)clang++

CFLAGS=-flto -O3 -v

LDFLAGS=-lm -v

.PHONY: cr clean

%.o: %.c cr
	$(CC) $(CFLAGS) -o $@ -c $<

simple: simple.o

simple2: simple2.o

vecadd: vecadd.o

vecadd_ptrswp: vecadd_ptrswp.o

jacobi: jacobi.o

%.bc: %.c
	$(CC) -O3 -c -emit-llvm $<

cr: checkpoint.bc restart.bc

clean:
	rm -rf .acriil_chkpnt-*
