LLVM_BIN_ROOT ?= ../../llvm-dbg/bin/

CC=$(LLVM_BIN_ROOT)clang
CXX=$(LLVM_BIN_ROOT)clang++

CFLAGS=-flto -O2

LDFLAGS=-lm -v

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

simple: simple.o

%.bc: %.c
	$(CC) -O3 -c -emit-llvm $<

cr: checkpoint.bc restart.bc

clean:
	rm *.bc simple.o simple.c

.PHONY: cr
