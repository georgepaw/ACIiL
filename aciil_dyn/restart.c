#include "cr.h"
#include <dirent.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char *__aciil_checkpoint_path;
int64_t __aciil_restart_checkpoint_file_counter = 0;
uint8_t **__aciil_pointer_alias_address;

uint64_t __aciil_get_checkpoint_before(char *base_path, int64_t epoch_before) {

  char *dir = ".";
  uint64_t most_recent_epoch = 0;
  // first need to find the most recent folder containing the checkpoints
  DIR *dp;
  struct dirent *entry;
  struct stat statbuf;
  if ((dp = opendir(dir)) == NULL) {
    fprintf(stderr, "cannot open directory: %s\n", dir);
    return -1;
  }

  chdir(dir);
  while ((entry = readdir(dp)) != NULL) {
    lstat(entry->d_name, &statbuf);
    if (S_ISDIR(statbuf.st_mode)) {
      /* Found a directory, but ignore . and .. */
      if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
        continue;
      if (strstr(entry->d_name, ".aciil_chkpnt-")) {
        char *end;
        uint64_t epoch =
            strtoull(entry->d_name + 14, // 14 is strlen of the prefix
                     &end, 10);
        if (end != entry->d_name + 14    // make sure it is a number
            && epoch > most_recent_epoch // and bigger than the last one
            && epoch < epoch_before)     // and smaller than the max
        {
          most_recent_epoch = epoch;
          strcpy(base_path, entry->d_name);
          base_path[entry->d_namlen] = '/';
          base_path[entry->d_namlen + 1] = '\0';
        }
      }
    }
  }
  closedir(dp);
  return most_recent_epoch;
}

int64_t __aciil_get_checkpoint_folder_before(char *checkpoint_dir,
                                             char *base_path,
                                             int64_t checkpoint_before) {
  DIR *dp;
  struct dirent *entry;
  struct stat statbuf;
  int64_t most_recent_checkpoint = -1;
  if ((dp = opendir(base_path)) == NULL) {
    fprintf(stderr, "cannot open directory: %s\n", base_path);
    return -1;
  }

  chdir(base_path);
  while ((entry = readdir(dp)) != NULL) {
    lstat(entry->d_name, &statbuf);
    if (S_ISDIR(statbuf.st_mode)) {
      /* Found a directory, but ignore . and .. */
      if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
        continue;
      char *end;
      int64_t checkpoint_num = strtoll(entry->d_name, &end, 10);
      if (entry->d_name != end && checkpoint_num > most_recent_checkpoint &&
          checkpoint_num < checkpoint_before) {
        most_recent_checkpoint = checkpoint_num;
        strcpy(checkpoint_dir, entry->d_name);
        checkpoint_dir[entry->d_namlen] = '/';
        checkpoint_dir[entry->d_namlen + 1] = '\0';
      }
    }
  }
  chdir("..");
  closedir(dp);
  return most_recent_checkpoint;
}

uint8_t __aciil_checkpoint_valid(int64_t num_variables) {
  for (int64_t i = 0; i < num_variables; i++) {
    char file_name[1024];
    sprintf(file_name, "%" PRIi64, i);
    char *file = __aciil_merge_char_arrays(__aciil_checkpoint_path, file_name);
    FILE *fp;
    fp = fopen(file, "r");
    if (!fp)
      return 0;
    // read the header
    uint64_t alias = 0;
    uint64_t size = 0;
    uint64_t num_elements = 0;
    char new_line;
    if (fscanf(fp, "alias%" PRIi64 "%c", &alias, &new_line) != 2 ||
        new_line != '\n') // read size
    {
      fclose(fp);
      return 0;
    }
    if (fscanf(fp, "%" PRIi64 "%c", &size, &new_line) != 2 ||
        new_line != '\n') // read size
    {
      fclose(fp);
      return 0;
    }
    if (fscanf(fp, "%" PRIi64 "%c", &num_elements, &new_line) != 2 ||
        new_line != '\n') // read num elements
    {
      fclose(fp);
      return 0;
    }

    // read the separator
    if (fscanf(fp, "%c", &new_line) != 1 || new_line != '\n') {
      fclose(fp);
      return 0;
    }

    if (alias) {
      uint64_t aliases_to;
      if (fscanf(fp, "%" PRIi64 "%c", &aliases_to, &new_line) != 2 ||
          new_line != '\n') // read aliases_to
      {
        fclose(fp);
        return 0;
      }
    } else {
      // read the data
      for (uint64_t i = 0; i < ROUND_BITS_TO_BYTES(size) * num_elements; i++) {
        char c;
        if (fscanf(fp, "%c", &c) != 1) // read char at a time
        {
          fclose(fp);
          return 0;
        }
      }
    }
    // read the EOF
    if (fscanf(fp, "%c", &new_line) != 1 || new_line != '\n') {
      fclose(fp);
      return 0;
    }
    fclose(fp);
  }
  return 1;
}

int64_t __aciil_restart_get_label() {
  char base_path[1024];
  char checkpoint_dir[1024];
  uint64_t epoch = UINT64_MAX;
  DIR *dp;
  struct dirent *entry;
  struct stat statbuf;
  int64_t label = -1;
  int64_t num_variables = -1;
  while (1) // todo fix this
  {
    epoch = __aciil_get_checkpoint_before(base_path, epoch);
    if (epoch == 0)
      return -1; // no checkpoints to use
    printf("Most recent epoch is %s\n", base_path);

    // now that the most recent application run was found
    // find the most recent checkpoint

    int64_t most_recent_checkpoint = INT64_MAX;
    while (1) {
      most_recent_checkpoint = __aciil_get_checkpoint_folder_before(
          checkpoint_dir, base_path, most_recent_checkpoint);
      if (most_recent_checkpoint < 0)
        break;

      __aciil_checkpoint_path =
          __aciil_merge_char_arrays(base_path, checkpoint_dir);

      if (!__aciil_checkpoint_path) {
        return -1;
      }
      printf("Checkpoint path is %s\n", __aciil_checkpoint_path);

      // open the info file which stores the info about the checkpoint
      FILE *fp;
      char *info_file_path =
          __aciil_merge_char_arrays(__aciil_checkpoint_path, "info");
      if (!info_file_path) {
        continue;
      }
      fp = fopen(info_file_path, "r");
      if (!fp) {
        continue;
      }
      // read the header
      if (fscanf(fp, "%" PRIi64 "\n", &label) != 1) // read the label number
      {
        fclose(fp);
        continue;
      }
      if (fscanf(fp, "%" PRIi64 "\n", &num_variables) !=
          1) // read the number of variables
      {
        fclose(fp);
        continue;
      }
      printf("Label is %" PRIi64 "\n", label);
      printf("Num variables is %" PRIi64 "\n", num_variables);

      fclose(fp);
      free(info_file_path);
      __aciil_restart_checkpoint_file_counter = 0;
      if (label >= 0 && __aciil_checkpoint_valid(num_variables)) {
        printf("*** ACIIL - Using checkpoint with label %" PRIi64 " ***\n",
               label);
        __aciil_pointer_alias_address =
            malloc(sizeof(uint8_t **) * num_variables);
        return label;
      } else {
        printf("Checkpoint invalid, trying an older version.\n");
      }
    }

    if (most_recent_checkpoint < 0)
      continue;
  }
  return -1;
}

void __aciil_restart_read_pointer_from_checkpoint(uint64_t size_bits,
                                                  uint64_t num_elements,
                                                  uint8_t *data) {
  char file_name[1024];
  sprintf(file_name, "%" PRIi64, __aciil_restart_checkpoint_file_counter);
  char *file = __aciil_merge_char_arrays(__aciil_checkpoint_path, file_name);
  FILE *fp;
  fp = fopen(file, "r");
  // read the header
  uint64_t alias_from_file = 0;
  uint64_t size_from_file = 0;
  uint64_t num_elements_from_file = 0;
  char new_line;
  if (fscanf(fp, "alias%" PRIi64 "%c", &alias_from_file, &new_line) != 2 ||
      new_line != '\n') // read size
  {
    printf("Restart has failed - header(alias) - aborted.\n");
    fclose(fp);
    exit(-1);
  }
  if (fscanf(fp, "%" PRIu64 "%c", &size_from_file, &new_line) != 2 ||
      new_line != '\n') // read size
  {
    printf("Restart has failed - header(size) - aborted.\n");
    fclose(fp);
    exit(-1);
  }
  if (fscanf(fp, "%" PRIu64 "%c", &num_elements_from_file, &new_line) != 2 ||
      new_line != '\n') // read num elements
  {
    printf("Restart has failed - header(num_elements) - aborted.\n");
    fclose(fp);
    exit(-1);
  }

  // read the separator
  if (fscanf(fp, "%c", &new_line) != 1 || new_line != '\n') {
    printf("Restart has failed - separator - aborted.\n");
    fclose(fp);
    exit(-1);
  }

  // read the data
  for (uint64_t i = 0; i < ROUND_BITS_TO_BYTES(size_bits) * num_elements; i++) {
    if (fscanf(fp, "%c", &data[i]) != 1) // read char at a time
    {
      printf("Restart has failed - body - aborted.\n");
      fclose(fp);
      exit(-1);
    }
  }
  if (fscanf(fp, "%c", &new_line) != 1 || new_line != '\n') {
    printf("Restart has failed - EOF - aborted. Char is %x\n", new_line);
    fclose(fp);
    exit(-1);
  }
  fclose(fp);
  free(file);
  __aciil_pointer_alias_address[__aciil_restart_checkpoint_file_counter] = data;
  printf("*** ACIIL - restored element %" PRIu64 " ***\n",
         __aciil_restart_checkpoint_file_counter);
  __aciil_restart_checkpoint_file_counter++;
}

uint8_t *__aciil_restart_read_alias_from_checkpoint(uint64_t size_bits,
                                                    uint64_t num_elements) {
  char file_name[1024];
  sprintf(file_name, "%" PRIi64, __aciil_restart_checkpoint_file_counter);
  char *file = __aciil_merge_char_arrays(__aciil_checkpoint_path, file_name);
  FILE *fp;
  fp = fopen(file, "r");
  // read the header
  uint64_t alias_from_file = 0;
  uint64_t size_from_file = 0;
  uint64_t num_elements_from_file = 0;
  uint64_t aliases_to = 0;
  char new_line;
  if (fscanf(fp, "alias%" PRIi64 "%c", &alias_from_file, &new_line) != 2 ||
      new_line != '\n') // read size
  {
    printf("Restart has failed - header(alias) - aborted.\n");
    fclose(fp);
    exit(-1);
  }
  if (fscanf(fp, "%" PRIu64 "%c", &size_from_file, &new_line) != 2 ||
      new_line != '\n') // read size
  {
    printf("Restart has failed - header(size) - aborted.\n");
    fclose(fp);
    exit(-1);
  }
  if (fscanf(fp, "%" PRIu64 "%c", &num_elements_from_file, &new_line) != 2 ||
      new_line != '\n') // read num elements
  {
    printf("Restart has failed - header(num_elements) - aborted.\n");
    fclose(fp);
    exit(-1);
  }

  // read the separator
  if (fscanf(fp, "%c", &new_line) != 1 || new_line != '\n') {
    printf("Restart has failed - separator - aborted.\n");
    fclose(fp);
    exit(-1);
  }
  // body
  if (fscanf(fp, "%" PRIu64 "%c", &aliases_to, &new_line) != 2 ||
      new_line != '\n') // aliases to
  {
    printf("Restart has failed - body(aliases_to) - aborted.\n");
    fclose(fp);
    exit(-1);
  }

  if (fscanf(fp, "%c", &new_line) != 1 || new_line != '\n') {
    printf("Restart has failed - EOF - aborted.\n");
    fclose(fp);
    exit(-1);
  }
  fclose(fp);
  free(file);

  uint8_t *out =
      __aciil_pointer_alias_address[__aciil_restart_checkpoint_file_counter] =
          __aciil_pointer_alias_address[aliases_to];
  printf("*** ACIIL - restored element %" PRIu64 " ***\n",
         __aciil_restart_checkpoint_file_counter);
  __aciil_restart_checkpoint_file_counter++;
  return out;
}

void __aciil_restart_finish() { free(__aciil_pointer_alias_address); }
