#ifndef CHECKPOINTRESTART_H
#define CHECKPOINTRESTART_H

#include <inttypes.h>
#include <iostream>
#include <map>
#include <string>

#define __ACRIIL_DEFAULT_CHECKPOINT_INTERVAL 100000000
#define deleteAndNull(x)                                                       \
  {                                                                            \
    delete x;                                                                  \
    x = NULL;                                                                  \
  }

class BTreeHeap {
public:
  BTreeHeap(uintptr_t ptr, uint64_t size) : ptr(ptr), size(size) {}
  uintptr_t ptr;
  uint64_t size;
  BTreeHeap *left = nullptr;
  BTreeHeap *right = nullptr;
};

class BTreeStack {
public:
  BTreeStack(uintptr_t ptr, uint64_t size) : ptr(ptr), size(size) {}
  uintptr_t ptr;
  uint64_t size;
  BTreeStack *left = nullptr;
  BTreeStack *right = nullptr;
};

class ACRIiLState {
  // checkpoint variables
  bool checkpointing = true;
  std::string *checkpointBaseDirectory;
  std::string *currentCheckpointDirectory;
  int64_t checkpointCounter = 0;
  int64_t currentCheckpointArgumentIndexCounter = 0;
  uint64_t checkpointInterval = 0;
  bool currentCheckpointEnabled = true;
  uint64_t nextCheckpointTime = 0;

  // restart variables
  uint8_t **restartPointerAliasAddresses;
  std::string *restartBaseDirectory;
  int64_t restartArgumentIndexCounter;

public:
  ~ACRIiLState();
  // memory
  std::map<uintptr_t, BTreeHeap *> heapMemory;
  BTreeHeap *heapMemoryRoot = nullptr;
  std::map<uintptr_t, BTreeStack *> stackMemory;
  BTreeStack *stackMemoryRoot = nullptr;
  uint64_t getTimeInMicroseconds();

  bool checkpointSetup();
  void checkpointStart();
  bool checkpointsEnabled();
  void permamentlyDisableCheckpointing();
  std::string &getCheckpointBaseDirectory();
  std::string &getCurrentCheckpointDirectory();
  std::string getNextCheckpointArgumentFileName();
  bool performCurrentCheckpoint();
  void stopCurrentCheckpoint();
  void updateNextCheckpointTime();
  void finishCheckpoint();

  void restartSetup(std::string dir, uint64_t numVariables);
  std::string &getRestartBaseDirectory();
  std::string getNextRestartArgumentFileName();
  void setAlias(uint8_t *ptr);
  uint8_t *getAlias(uint64_t aliasesTo);
  void restartFinish();
};

ACRIiLState state;

// putting extern C is a way to make sure the functions names do not get mangled
// and that they are easy to dynamically load in LLVM
// checkpoint extern functions
extern "C" void __acriilCheckpointerAddHeapMemoryAllocation(char *ptr,
                                                            uint64_t size);
extern "C" void __acriilCheckpointerAddStackMemoryAllocation(char *ptr,
                                                             uint64_t size);
extern "C" void __acriilCheckpointSetup();
extern "C" void __acriilCheckpointStart(int64_t labelNumber,
                                        int64_t numVariablesToCheckpoint);
extern "C" void __acriilCheckpointPointer(uint64_t elementSizeBits,
                                          uint64_t numElements, char *data);
extern "C" void __acriilCheckpointAlias(uint64_t numCandidates,
                                        uint64_t elementSizeBits,
                                        uint64_t numElements,
                                        char *currentPointer, ...);
extern "C" void __acriilCheckpointFinish();

// restart extern functions
extern "C" uint8_t *
__acriilRestartReadAliasFromCheckpoint(uint64_t sizeBits, uint64_t numElements);
extern "C" void __acriilRestartReadPointerFromCheckpoint(uint64_t sizeBits,
                                                         uint64_t numElements,
                                                         uint8_t *data);
extern "C" int64_t __acriilRestartGetLabel();
extern "C" void __acriilRestartFinish();
#endif
