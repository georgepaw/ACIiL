#include "checkpointRestart.h"
#include <inttypes.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

ACRIiLState::~ACRIiLState() {
  deleteAndNull(checkpointBaseDirectory);
  deleteAndNull(currentCheckpointDirectory);
  deleteAndNull(restartBaseDirectory);
  for (auto pair : heapMemory) {
    deleteAndNull(pair.second);
  }
  heapMemory.clear();
  for (auto pair : stackMemory) {
    deleteAndNull(pair.second);
  }
  stackMemory.clear();
}

uint64_t ACRIiLState::getTimeInMicroseconds() {
  struct timespec tms;
  if (clock_gettime(CLOCK_MONOTONIC, &tms)) {
    permamentlyDisableCheckpointing();
    return 0;
  }
  uint64_t micros = tms.tv_sec * 1000000 + tms.tv_nsec / 1000;
  return micros;
}

bool ACRIiLState::checkpointSetup() {
  checkpointing = true;
  // get the current time in microseconds
  uint64_t currentTime = getTimeInMicroseconds();
  // if there is an env var with the interval use that
  if (const char *interval = std::getenv("ACRIIL_CHECKPOINT_INTERVAL")) {
    char *end;
    double val = strtod(interval, &end);
    if (interval != end) {
      checkpointInterval = val * 1000000;
    }
  } else {
    checkpointInterval = __ACRIIL_DEFAULT_CHECKPOINT_INTERVAL;
  }

  std::cerr << "*** ACRIiL - checkpoint interval is " << std::fixed
            << std::setprecision(2) << ((double)checkpointInterval) / 1000000.0
            << "s ***" << std::endl;

  // set up the base path
  deleteAndNull(checkpointBaseDirectory);
  checkpointBaseDirectory =
      new std::string(".acriil_chkpnt-" + std::to_string(currentTime) + "/");

  updateNextCheckpointTime();
  return checkpointsEnabled();
}

bool ACRIiLState::checkpointsEnabled() { return checkpointing; }

void ACRIiLState::checkpointStart() {
  if (getTimeInMicroseconds() < nextCheckpointTime) {
    stopCurrentCheckpoint();
    return;
  }

  deleteAndNull(currentCheckpointDirectory);
  currentCheckpointDirectory = new std::string(
      getCheckpointBaseDirectory() + std::to_string(checkpointCounter));

  currentCheckpointArgumentIndexCounter = 0;
  currentCheckpointEnabled = true;
}

void ACRIiLState::permamentlyDisableCheckpointing() { checkpointing = false; }

std::string &ACRIiLState::getCheckpointBaseDirectory() {
  return *checkpointBaseDirectory;
}

std::string &ACRIiLState::getCurrentCheckpointDirectory() {
  return *currentCheckpointDirectory;
}

void ACRIiLState::finishCheckpoint() {
  if (performCurrentCheckpoint()) {
    checkpointCounter++;
    updateNextCheckpointTime();
  }
}

std::string ACRIiLState::getNextCheckpointArgumentFileName() {
  return std::string(getCurrentCheckpointDirectory() + "/" +
                     std::to_string(currentCheckpointArgumentIndexCounter++));
}

bool ACRIiLState::performCurrentCheckpoint() {
  return checkpointsEnabled() && currentCheckpointEnabled;
}
void ACRIiLState::stopCurrentCheckpoint() { currentCheckpointEnabled = false; }

void ACRIiLState::updateNextCheckpointTime() {
  nextCheckpointTime = getTimeInMicroseconds() + checkpointInterval;
}

void ACRIiLState::restartSetup(std::string dir, uint64_t numVariables) {
  restartArgumentIndexCounter = -1;
  deleteAndNull(restartBaseDirectory);
  restartBaseDirectory = new std::string(dir);
  restartPointerAliasAddresses =
      (uint8_t **)malloc(sizeof(uint8_t **) * numVariables);
}

std::string &ACRIiLState::getRestartBaseDirectory() {
  return *restartBaseDirectory;
}

std::string ACRIiLState::getNextRestartArgumentFileName() {
  return std::string(getRestartBaseDirectory() + "/" +
                     std::to_string(++restartArgumentIndexCounter));
}

void ACRIiLState::setAlias(uint8_t *ptr) {
  restartPointerAliasAddresses[restartArgumentIndexCounter] = ptr;
}

uint8_t *ACRIiLState::getAlias(uint64_t aliasesTo) {
  return restartPointerAliasAddresses[aliasesTo];
}

void ACRIiLState::restartFinish() {
  free(restartPointerAliasAddresses);
  std::cerr << "*** ACRIiL - Restart finished ***" << std::endl;
  updateNextCheckpointTime();
}
