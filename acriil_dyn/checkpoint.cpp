#include "checkpointRestart.h"
#include <fstream>
#include <inttypes.h>
#include <iostream>
#include <map>
#include <stdarg.h>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>

void __acriilInsertHeap(BTreeHeap *&root, BTreeHeap *leaf) {
  if (!root) {
    root = leaf;
    state.heapMemory[leaf->ptr] = leaf;
  } else if (leaf->ptr < root->ptr) {
    __acriilInsertHeap(root->left, leaf);
  } else {
    __acriilInsertHeap(root->right, leaf);
  }
}

void __acriilDumpHeapAllocs() {
  for (auto p : state.heapMemory) {
    std::cout << "ptr " << p.second->ptr;
    if (p.second->left)
      std::cout << " left " << p.second->left->ptr;
    if (p.second->right)
      std::cout << " right " << p.second->right->ptr;
    std::cout << std::endl;
  }
  std::cout << std::endl;
}

void __acriilInsertStack(BTreeStack *&root, BTreeStack *leaf) {
  if (!root) {
    root = leaf;
    state.stackMemory[leaf->ptr] = leaf;
  } else if (leaf->ptr < root->ptr) {
    __acriilInsertStack(root->left, leaf);
  } else {
    __acriilInsertStack(root->right, leaf);
  }
}

void __acriilDumpStackAllocs() {
  for (auto p : state.stackMemory) {
    std::cout << "ptr " << p.second->ptr;
    if (p.second->left)
      std::cout << " left " << p.second->left->ptr;
    if (p.second->right)
      std::cout << " right " << p.second->right->ptr;
    std::cout << std::endl;
  }
  std::cout << std::endl;
}

void __acriilCheckpointerAddHeapMemoryAllocation(char *ptr, uint64_t size) {
  __acriilInsertHeap(state.heapMemoryRoot, new BTreeHeap((uintptr_t)ptr, size));
}

void __acriilCheckpointerAddStackMemoryAllocation(char *ptr, uint64_t size) {
  __acriilInsertStack(state.stackMemoryRoot,
                      new BTreeStack((uintptr_t)ptr, size));
}

void __acriilCheckpointSetup() {
  if (!state.checkpointSetup())
    return;

  // create the checkpoint directory
  // if there are any problems, no checkpointing should be done
  struct stat st = {0};
  if (stat(state.getCheckpointBaseDirectory().c_str(), &st) !=
          -1 // check that it doesn't already exists
      || mkdir(state.getCheckpointBaseDirectory().c_str(), 0700) ==
             -1) // and that we can create it
  {
    state.permamentlyDisableCheckpointing();
    std::cerr << "*** ACRIiL - Could not create the checkpoint directory "
              << state.getCheckpointBaseDirectory()
              << ", checkpointing will "
                 "not be performed"
              << std::endl;
    return;
  }
}

void __acriilCheckpointStart(int64_t labelNumber,
                             int64_t numVariablesToCheckpoint) {
  state.checkpointStart();
  if (!state.performCurrentCheckpoint())
    return;

  std::cerr << "*** ACRIiL - checkpoint start ***" << std::endl;

  // for every checkpoint create a directory that stores all the files
  struct stat st = {0};

  if (stat(state.getCurrentCheckpointDirectory().c_str(), &st) !=
          -1 // check that it doesn't already exists
      || mkdir(state.getCurrentCheckpointDirectory().c_str(), 0700) ==
             -1) // and that we can create it
  {
    state.stopCurrentCheckpoint();
    std::cerr << "*** ACRIiL - Could not create the checkpoint directory "
              << state.getCurrentCheckpointDirectory()
              << ", checkpointing will "
                 "not be performed"
              << std::endl;
    return;
  }
  // write the info about the checkpoint to a info file
  std::string fileName = state.getCurrentCheckpointDirectory() + "/info";

  // write to a file
  std::ofstream file;
  file.open(fileName, std::ios::out);
  // first write the header
  file << labelNumber << "\n";              // label number
  file << numVariablesToCheckpoint << "\n"; // number of live variables
  file.close();
}

void __acriilCheckpointPointer(uint64_t elementSizeBits, uint64_t numElements,
                               char *data) {
  if (!state.performCurrentCheckpoint())
    return;

  // for the data passed in
  // dump it to a file
  std::string fileName = state.getNextCheckpointArgumentFileName();

  const uint64_t alias = 0;

  // write to a file
  std::ofstream file;
  file.open(fileName, std::ios::out);
  // first write the header
  file << "alias " << alias
       << "\n"; // indicates whether this is alias checkpoint or actual data
  file << elementSizeBits << "\n"; // size in bits
  file << numElements << "\n";     // num elements

  // write a separator
  file << "\n";

  // body
  // dump the binary data (round to a byte size)
  const uint64_t total_bits = elementSizeBits * numElements;
  for (uint64_t i = 0; i < total_bits; i += 8) {
    file.write(&data[i / 8], 1);
  }
  file << "\n";

  file.close();
}

void __acriilCheckpointAlias(uint64_t numCandidates, uint64_t elementSizeBits,
                             uint64_t numElements, char *currentPointer, ...) {
  if (!state.performCurrentCheckpoint())
    return;

  // for the data passed in
  // dump it to a file
  std::string fileName = state.getNextCheckpointArgumentFileName();

  va_list args;
  va_start(args, currentPointer);

  uint64_t referanceLabel = 0;
  bool foundAlias = false;
  for (uint64_t i = 0; i < numCandidates && !foundAlias; i++) {
    uint64_t aliasElementSizeBits = va_arg(args, uint64_t);
    uint64_t aliasNumElements = va_arg(args, uint64_t);
    uint64_t aliasReferanceLabel = va_arg(args, uint64_t);
    char *aliasPointerStart = va_arg(args, char(*));
    if (aliasPointerStart == currentPointer) {
      foundAlias = true;
      referanceLabel = aliasReferanceLabel;
    }
  }
  if (!foundAlias) {
    state.stopCurrentCheckpoint();
    std::cerr << "*** ACRIiL - Could not checkpoint an alias, checkpointing "
                 "will not be performed"
              << std::endl;
  }
  // if (foundAlias)
  //   printf("Pointer aliases the %" PRIu64 " pointer" << std::endl, ref);
  va_end(args);

  const uint64_t alias = 1;

  // write to a file
  std::ofstream file;
  file.open(fileName, std::ios::out);
  // first write the header
  file << "alias " << alias
       << "\n"; // indicates whether this is alias checkpoint or actual data
  file << elementSizeBits << "\n"; // size in bits
  file << numElements << "\n";     // num elements

  // write a separator
  file << "\n";

  // body
  file << referanceLabel << "\n"; // write which pointer is aliased

  file.close();
}

void __acriilCheckpointFinish() {
  state.finishCheckpoint();

  if (state.performCurrentCheckpoint()) {
    std::cerr << "*** ACRIiL - checkpoint finish ***" << std::endl;
  }
}
