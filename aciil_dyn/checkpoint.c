#include "cr.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define __ACIIL_DEFAULT_CHECKPOINT_INTERVAL 100

char __aciil_base_path[1024];
char __aciil_checkpoint_path[1024];
int64_t __aciil_checkpoint_counter = 0;
int64_t __aciil_argument_counter = 0;
uint8_t __aciil_perform_checkpoint = 1;
uint64_t __aciil_checkpoint_interval = __ACIIL_DEFAULT_CHECKPOINT_INTERVAL;
uint8_t __aciil_checkpoint_skip = 1;
uint64_t __aciil_next_checkpoint_time;

inline uint64_t __aciil_get_time_in_ms() {
  struct timespec tms;
  if (clock_gettime(CLOCK_REALTIME, &tms)) {
    __aciil_perform_checkpoint = 0;
    return 0;
  }
  return tms.tv_sec * 1000000 + tms.tv_nsec / 1000;
}

void __aciil_checkpoint_setup() {
  // get the current time in microseconds
  uint64_t micros = __aciil_get_time_in_ms();
  if (!__aciil_perform_checkpoint)
    return;

  // if there is an env var with the interval use that
  const char *s = getenv("ACIIL_CHECKPOINT_INTERVAL");
  if (s) {
    char *end;
    uint64_t val = strtoull(s, &end, 10);
    if (s != end) {
      __aciil_checkpoint_interval = val;
    }
  }
  printf("*** ACIIL - checkpoint interval is %" PRIu64 "ms ***\n",
         __aciil_checkpoint_interval);

  __aciil_next_checkpoint_time = micros + __aciil_checkpoint_interval;
  __aciil_checkpoint_skip = 1;

  // set up the base path
  sprintf(__aciil_base_path, ".aciil_chkpnt-%" PRIu64 "/", micros);

  // create the directory,
  // if there are any problems, no checkpointing should be done
  struct stat st = {0};
  if (stat(__aciil_base_path, &st) != -1 // check that it doesn't already exists
      || mkdir(__aciil_base_path, 0700) == -1) // and that we can create it
  {
    __aciil_perform_checkpoint = 0;
    printf("Could not create the checkpoint directory %s, checkpointing will "
           "not be performed\n",
           __aciil_checkpoint_path);
    return;
  }
}

void __aciil_checkpoint_start(int64_t label_number, int64_t num_variables) {
  if (!__aciil_perform_checkpoint)
    return;

  // check if it's time to checkpoint
  if (__aciil_get_time_in_ms() < __aciil_next_checkpoint_time) {
    __aciil_checkpoint_skip = 1;
    return;
  } else {
    __aciil_checkpoint_skip = 0;
  }

  printf("*** ACIIL - checkpoint start ***\n");

  // for every checkpoint create a directory that stores all the files
  sprintf(__aciil_checkpoint_path, "%s%" PRIi64 "/", __aciil_base_path,
          __aciil_checkpoint_counter);
  struct stat st = {0};

  if (stat(__aciil_checkpoint_path, &st) !=
          -1 // check that it doesn't already exists
      ||
      mkdir(__aciil_checkpoint_path, 0700) == -1) // and that we can create it
  {
    __aciil_checkpoint_skip = 1;
    printf("Could not create the checkpoint directory %s, checkpointing will "
           "not be performed\n",
           __aciil_checkpoint_path);
    return;
  }
  __aciil_argument_counter = 0;
  // write the info about the checkpoint to a info file
  char *file_path = __aciil_merge_char_arrays(__aciil_checkpoint_path, "info");

  // write to a file
  FILE *fp;
  fp = fopen(file_path, "w");
  // first write the header
  fprintf(fp, "%" PRIi64 "\n", label_number);  // label number
  fprintf(fp, "%" PRIi64 "\n", num_variables); // label number

  fclose(fp);
  free(file_path);
}

void __aciil_checkpoint_pointer(int64_t sizeBits, char *data) {
  if (!__aciil_perform_checkpoint || __aciil_checkpoint_skip)
    return;

  // for the data passed in
  // dump it to a file
  char file_name[255];
  sprintf(file_name, "%" PRIi64, __aciil_argument_counter);
  char *file_path =
      __aciil_merge_char_arrays(__aciil_checkpoint_path, file_name);
  if (!file_path) {
    __aciil_checkpoint_skip = 1;
    printf("Could not create the checkpoint directory %s, checkpointing will "
           "not be performed\n",
           __aciil_checkpoint_path);
  }

  // write to a file
  FILE *fp;
  fp = fopen(file_path, "w");
  // first write the header
  fprintf(fp, "%" PRIi64 "\n", sizeBits); // size in bits

  // write a separator
  fprintf(fp, "\n");

  // then dump the binary data (round to a byte size)
  for (int64_t i = 0; i < ROUND_BITS_TO_BYTES(sizeBits); i++) {
    fprintf(fp, "%c", data[i]);
  }
  fprintf(fp, "\n");

  fclose(fp);
  __aciil_argument_counter++;
  free(file_path);
}

void __aciil_checkpoint_finish() {
  __aciil_checkpoint_counter++;

  __aciil_next_checkpoint_time =
      __aciil_get_time_in_ms() + __aciil_checkpoint_interval;

  if (!__aciil_checkpoint_skip)
    printf("*** ACIIL - checkpoint finish ***\n");
}
