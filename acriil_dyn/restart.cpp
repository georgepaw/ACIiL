#include "checkpointRestart.h"
#include <dirent.h>
#include <fstream>
#include <inttypes.h>
#include <iostream>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

std::set<std::string> __acriilGetAllFiles(std::string path) {
  char currentDir[1024];
  getcwd(currentDir, 1024);

  std::set<std::string> fileNames;

  DIR *dp;
  struct dirent *entry;
  struct stat statbuf;
  if ((dp = opendir(path.c_str())) == NULL) {
    std::cerr << "*** ACRIIL - cannot open directory " << path << " ***"
              << std::endl;
    return fileNames;
  }

  chdir(path.c_str());
  while ((entry = readdir(dp)) != NULL) {
    lstat(entry->d_name, &statbuf);
    if (S_ISDIR(statbuf.st_mode)) {
      /* Found a directory, but ignore . and .. */
      std::string fileName(entry->d_name);
      if (fileName != "." && fileName != "..") {
        fileNames.insert(fileName);
      }
    }
  }
  chdir(currentDir);
  closedir(dp);
  return fileNames;
}

bool __acriilCheckpointValid(int64_t &labelNumber, uint64_t &numVariables,
                             std::string checkpointDir) {

  // open the info file which stores the info about the checkpoint
  std::string infoFileName = checkpointDir + "/info";
  std::ifstream infoFile;
  infoFile.open(infoFileName, std::ios::in);
  if (!infoFile.is_open())
    return false;

  // read the header
  if (!(infoFile >> labelNumber >> std::ws)) // read the label number
    return false;
  if (!(infoFile >> numVariables >> std::ws)) // read the number of variables
    return false;

  std::cerr << "*** ACRIiL Label is " << labelNumber << " ***" << std::endl;
  std::cerr << "*** ACRIiL Num variables " << numVariables << " ***"
            << std::endl;

  if (labelNumber < 0)
    return false;
  infoFile.close();
  // now verify files
  for (uint64_t i = 0; i < numVariables; i++) {
    std::string fileName = checkpointDir + "/" + std::to_string(i);
    std::ifstream file;
    file.open(fileName, std::ios::in);
    if (!file.is_open())
      return false;

    // read the header
    std::string aliasString;
    uint64_t alias = 0;
    uint64_t sizeBits = 0;
    uint64_t numElements = 0;

    if (!(file >> aliasString >> alias >> std::ws) ||
        aliasString != "alias") // read alias
      return false;
    if (!(file >> sizeBits >> std::ws))
      return false;
    if (!(file >> numElements >> std::ws))
      return false;

    // read the separator
    if (!(file >> std::ws))
      return false;

    if (alias) {
      uint64_t aliasesTo;
      if (!(file >> aliasesTo))
        return false;
    } else {
      const uint64_t totalBits = sizeBits * numElements;
      char c;
      // read the data
      for (uint64_t i = 0; i < totalBits; i += 8)
        if (!(file.read(&c, 1))) // read char at a time
          return false;
    }
    if (!(file >> std::ws))
      return false;
    if (!file.eof())
      return false;
    file.close();
  }
  return true;
}

int64_t __acriilRestartGetLabel() {
  int64_t labelNumber = -1;
  // TODO this should be in a header
  std::string checkpointPrefix(".acriil_chkpnt-");
  // first get all the files in current dir
  std::set<std::string> currentDir = __acriilGetAllFiles(".");
  std::set<uint64_t> epochs;
  for (std::string fileName : currentDir) {
    if (fileName.compare(0, checkpointPrefix.size(), checkpointPrefix) == 0) {
      char *end;
      uint64_t epoch =
          strtoull(fileName.c_str() + checkpointPrefix.size(), &end, 10);
      if (end != fileName.c_str() + checkpointPrefix.size()) {
        epochs.insert(epoch);
      }
    }
  }
  bool foundValidCheckpoint = false;
  // iterate over epochs from most recent to find the most recent valid
  // checkpoint
  for (std::set<uint64_t>::reverse_iterator rit = epochs.rbegin();
       rit != epochs.rend() && !foundValidCheckpoint; rit++) {
    std::string checkpointsDir = checkpointPrefix + std::to_string(*rit);
    std::cerr << "*** ACRIIL - Looking for checkpoints in " << checkpointsDir
              << " ***" << std::endl;
    std::set<std::string> checkpointDirs = __acriilGetAllFiles(checkpointsDir);
    std::set<uint64_t> checkpoints;
    for (std::string fileName : checkpointDirs) {
      char *end;
      uint64_t checkpoint = strtoull(fileName.c_str(), &end, 10);
      if (end != fileName.c_str()) {
        checkpoints.insert(checkpoint);
      }
    }

    // iterate over checkpoints
    for (std::set<uint64_t>::reverse_iterator rit = checkpoints.rbegin();
         rit != checkpoints.rend() && !foundValidCheckpoint; rit++) {
      std::string checkpointDir = checkpointsDir + "/" + std::to_string(*rit);
      std::cerr << "*** ACRIIL - Verifying checkpoint in " << checkpointDir
                << " ***" << std::endl;

      uint64_t numVariables = 0;
      int64_t label = 0;
      if (__acriilCheckpointValid(label, numVariables, checkpointDir)) {
        std::cerr << "*** ACRIiL - Using checkpoint with label " << label
                  << " ***" << std::endl;
        labelNumber = label;
        state.restartSetup(checkpointDir, numVariables);
        foundValidCheckpoint = true;
      } else {
        std::cerr
            << "*** ACRIiL - Checkpoint invalid, trying an older version. ***"
            << std::endl;
      }
    }
  }
  return labelNumber;
}

void __acriilRestartReadPointerFromCheckpoint(uint64_t sizeBits,
                                              uint64_t numElements,
                                              uint8_t *data) {
  std::string fileName = state.getNextRestartArgumentFileName();
  std::ifstream file;
  file.open(fileName, std::ios::in);
  if (!file.is_open())
    exit(-1);

  // read the header
  std::string aliasString;
  uint64_t aliasFromFile = 0;
  uint64_t sizeBitsFromFile = 0;
  uint64_t numElementsFromFile = 0;

  if (!(file >> aliasString >> aliasFromFile >> std::ws) ||
      aliasString != "alias" || aliasFromFile != 0) { // read alias
    std::cerr << "*** ACRIiL - Restart has failed - header(alias) - aborted ***"
              << std::endl;
    exit(-1);
  }
  if (!(file >> sizeBitsFromFile >> std::ws)) { // read sizebits
    std::cerr
        << "*** ACRIiL - Restart has failed - header(sizeBits) - aborted ***"
        << std::endl;
    exit(-1);
  }
  if (!(file >> numElementsFromFile >> std::ws)) { // read numElements
    std::cerr
        << "*** ACRIiL - Restart has failed - header(numElements) - aborted ***"
        << std::endl;
    exit(-1);
  }

  // read the separator
  if (!(file >> std::ws)) {
    std::cerr << "*** ACRIiL - Restart has failed - separator - aborted ***"
              << std::endl;
    exit(-1);
  }

  // read the data
  const uint64_t totalBits = sizeBits * numElements;
  for (uint64_t i = 0; i < totalBits; i += 8) {
    char c;
    if (!(file.read(&c, 1))) // read char at a time
    {
      std::cerr << "*** ACRIiL - Restart has failed - body - aborted ***"
                << std::endl;
      exit(-1);
    }
    // make sure to not overwrite other data when writing less than a byte
    if (i + 8 <= totalBits) {
      data[i / 8] = c;
    } else {
      uint64_t diff = totalBits - i;
      uint8_t mask = (~0 << diff);
      data[i / 8] = (data[i / 8] & mask) | (c & ~mask);
    }
  }

  if (!(file >> std::ws)) {
    std::cerr << "*** ACRIiL - Restart has failed - EOF - aborted ***"
              << std::endl;
    exit(-1);
  }
  file.close();
  state.setAlias(data);
}

uint8_t *__acriilRestartReadAliasFromCheckpoint(uint64_t sizeBits,
                                                uint64_t numElements) {
  std::string fileName = state.getNextRestartArgumentFileName();
  std::ifstream file;
  file.open(fileName, std::ios::in);
  if (!file.is_open())
    exit(-1);

  // read the header
  std::string aliasString;
  uint64_t aliasFromFile = 0;
  uint64_t sizeBitsFromFile = 0;
  uint64_t numElementsFromFile = 0;
  uint64_t aliasesTo;

  if (!(file >> aliasString >> aliasFromFile >> std::ws) ||
      aliasString != "alias" || aliasFromFile != 1) { // read alias
    std::cerr << "*** ACRIiL - Restart has failed - header(alias) - aborted ***"
              << std::endl;
    exit(-1);
  }
  if (!(file >> sizeBitsFromFile >> std::ws)) { // read sizebits
    std::cerr
        << "*** ACRIiL - Restart has failed - header(sizeBits) - aborted ***"
        << std::endl;
    exit(-1);
  }
  if (!(file >> numElementsFromFile >> std::ws)) { // read numElements
    std::cerr
        << "*** ACRIiL - Restart has failed - header(numElements) - aborted ***"
        << std::endl;
    exit(-1);
  }

  // read the separator
  if (!(file >> std::ws)) {
    std::cerr << "*** ACRIiL - Restart has failed - separator - aborted ***"
              << std::endl;
    exit(-1);
  }

  // body
  if (!(file >> aliasesTo)) { // read numElements
    std::cerr
        << "*** ACRIiL - Restart has failed - header(aliasesTo) - aborted ***"
        << std::endl;
    exit(-1);
  }

  if (!(file >> std::ws)) {
    std::cerr << "*** ACRIiL - Restart has failed - EOF - aborted ***"
              << std::endl;
    exit(-1);
  }
  file.close();

  uint8_t *out = state.getAlias(aliasesTo);
  state.setAlias(out);
  return out;
}

void __acriilRestartFinish() { state.restartFinish(); }
