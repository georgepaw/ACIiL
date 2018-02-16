#include "cr.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define __ACRIIL_DEFAULT_CHECKPOINT_INTERVAL 100

char __acriil_base_path[1024];
char __acriil_checkpoint_path[1024];
int64_t __acriil_checkpoint_counter = 0;
int64_t __acriil_argument_counter = 0;
uint8_t __acriil_perform_checkpoint = 1;
uint64_t __acriil_checkpoint_interval = __ACRIIL_DEFAULT_CHECKPOINT_INTERVAL;
uint8_t __acriil_checkpoint_skip = 1;
uint64_t __acriil_next_checkpoint_time;

inline uint64_t __acriil_get_time_in_ms() {
  struct timespec tms;
  if (clock_gettime(CLOCK_MONOTONIC, &tms)) {
    __acriil_perform_checkpoint = 0;
    return 0;
  }
  uint64_t micros = tms.tv_sec * 1000000 + tms.tv_nsec / 1000;
  return micros;
}

void __acriil_checkpoint_setup() {
  // get the current time in microseconds
  uint64_t micros = __acriil_get_time_in_ms();
  if (!__acriil_perform_checkpoint)
    return;

  // if there is an env var with the interval use that
  const char *s = getenv("ACRIIL_CHECKPOINT_INTERVAL");
  if (s) {
    char *end;
    uint64_t val = strtoull(s, &end, 10);
    if (s != end) {
      __acriil_checkpoint_interval = val;
    }
  }
  printf("*** ACRIiL - checkpoint interval is %" PRIu64 "micros ***\n",
         __acriil_checkpoint_interval);

  __acriil_next_checkpoint_time = micros + __acriil_checkpoint_interval;
  __acriil_checkpoint_skip = 1;

  // set up the base path
  sprintf(__acriil_base_path, ".acriil_chkpnt-%" PRIu64 "/", micros);

  // create the directory,
  // if there are any problems, no checkpointing should be done
  struct stat st = {0};
  if (stat(__acriil_base_path, &st) !=
          -1 // check that it doesn't already exists
      || mkdir(__acriil_base_path, 0700) == -1) // and that we can create it
  {
    __acriil_perform_checkpoint = 0;
    printf("Could not create the checkpoint directory %s, checkpointing will "
           "not be performed\n",
           __acriil_checkpoint_path);
    return;
  }
}

void __acriil_checkpoint_start(int64_t label_number, int64_t num_variables) {
  if (!__acriil_perform_checkpoint)
    return;

  // check if it's time to checkpoint
  if (__acriil_get_time_in_ms() < __acriil_next_checkpoint_time) {
    __acriil_checkpoint_skip = 1;
    return;
  } else {
    __acriil_checkpoint_skip = 0;
  }

  printf("*** ACRIiL - checkpoint start ***\n");

  // for every checkpoint create a directory that stores all the files
  sprintf(__acriil_checkpoint_path, "%s%" PRIi64 "/", __acriil_base_path,
          __acriil_checkpoint_counter);
  struct stat st = {0};

  if (stat(__acriil_checkpoint_path, &st) !=
          -1 // check that it doesn't already exists
      ||
      mkdir(__acriil_checkpoint_path, 0700) == -1) // and that we can create it
  {
    __acriil_checkpoint_skip = 1;
    printf("Could not create the checkpoint directory %s, checkpointing will "
           "not be performed\n",
           __acriil_checkpoint_path);
    return;
  }
  __acriil_argument_counter = 0;
  // write the info about the checkpoint to a info file
  char *file_path =
      __acriil_merge_char_arrays(__acriil_checkpoint_path, "info");

  // write to a file
  FILE *fp;
  fp = fopen(file_path, "w");
  // first write the header
  fprintf(fp, "%" PRIi64 "\n", label_number);  // label number
  fprintf(fp, "%" PRIi64 "\n", num_variables); // label number

  fclose(fp);
  free(file_path);
}

void __acriil_checkpoint_pointer(uint64_t element_size_bits,
                                 uint64_t num_elements, char *data) {
  if (!__acriil_perform_checkpoint || __acriil_checkpoint_skip)
    return;

  // for the data passed in
  // dump it to a file
  char file_name[255];
  sprintf(file_name, "%" PRIi64, __acriil_argument_counter);
  char *file_path =
      __acriil_merge_char_arrays(__acriil_checkpoint_path, file_name);
  if (!file_path) {
    __acriil_checkpoint_skip = 1;
    printf("Could not create the checkpoint file, checkpointing will "
           "not be performed\n");
  }

  const uint64_t alias = 0;

  // write to a file
  FILE *fp;
  fp = fopen(file_path, "w");
  // first write the header
  fprintf(fp, "alias%" PRIu64 "\n",
          alias); // indicates whether this is alias checkpoint or actual data
  fprintf(fp, "%" PRIu64 "\n", element_size_bits); // size in bits
  fprintf(fp, "%" PRIu64 "\n", num_elements);      // num elements

  // write a separator
  fprintf(fp, "\n");

  // then dump the binary data (round to a byte size)
  for (uint64_t i = 0;
       i < ROUND_BITS_TO_BYTES(element_size_bits) * num_elements; i++) {
    fprintf(fp, "%c", data[i]);
  }
  fprintf(fp, "\n");

  fclose(fp);
  __acriil_argument_counter++;
  free(file_path);
}

void __acriil_checkpoint_alias(uint64_t num_candidates,
                               uint64_t element_size_bits,
                               uint64_t num_elements, char *current_pointer,
                               ...) {
  if (!__acriil_perform_checkpoint || __acriil_checkpoint_skip)
    return;

  // for the data passed in
  // dump it to a file
  char file_name[255];
  sprintf(file_name, "%" PRIi64, __acriil_argument_counter);
  char *file_path =
      __acriil_merge_char_arrays(__acriil_checkpoint_path, file_name);
  if (!file_path) {
    __acriil_checkpoint_skip = 1;
    printf("Could not create the checkpoint file, checkpointing will "
           "not be performed\n");
  }

  uint8_t found_alias = 0;
  va_list args;
  va_start(args, current_pointer);

  uint64_t ref = 0;
  for (uint64_t i = 0; i < num_candidates && !found_alias; i++) {
    uint64_t alias_element_size_bits = va_arg(args, uint64_t);
    uint64_t alias_num_elements = va_arg(args, uint64_t);
    uint64_t alias_ref = va_arg(args, uint64_t);
    char *pointer_start = va_arg(args, char(*));
    // printf("0x%" PRIXPTR " 0x%" PRIXPTR "\n", (uintptr_t)current_pointer,
    //        (uintptr_t)pointer_start);
    if (pointer_start == current_pointer) {
      found_alias = 1;
      ref = alias_ref;
    }
  }
  if (!found_alias) {
    __acriil_checkpoint_skip = 1;
    printf("Could not checkpoint an alias, checkpointing will "
           "not be performed\n");
  }
  // if (found_alias)
  //   printf("Pointer aliases the %" PRIu64 " pointer\n", ref);
  va_end(args);

  const uint64_t alias = 1;

  // write to a file
  FILE *fp;
  fp = fopen(file_path, "w");
  // first write the header
  fprintf(fp, "alias%" PRIu64 "\n",
          alias); // indicates whether this is alias checkpoint or actual data
  fprintf(fp, "%" PRIu64 "\n", element_size_bits); // size in bits
  fprintf(fp, "%" PRIu64 "\n", num_elements);      // num elements

  // write a separator
  fprintf(fp, "\n");

  // body
  // write which pointer is aliased
  fprintf(fp, "%" PRIu64 "\n", ref);
  fprintf(fp, "\n");

  fclose(fp);
  __acriil_argument_counter++;
  free(file_path);
}

void __acriil_checkpoint_finish() {
  __acriil_checkpoint_counter++;

  if (!__acriil_checkpoint_skip) {
    printf("*** ACRIiL - checkpoint finish ***\n");
    __acriil_next_checkpoint_time =
        __acriil_get_time_in_ms() + __acriil_checkpoint_interval;
  }
}
