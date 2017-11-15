#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <inttypes.h>
#include "cr.h"

char * __aciil_checkpoint_path;
int64_t __aciil_restart_checkpoint_file_counter = 0;

uint64_t __aciil_get_checkpoint_before(char * base_path, int64_t epoch_before)
{

  char * dir = ".";
  uint64_t most_recent_epoch = 0;
  //first need to find the most recent folder containing the checkpoints
  DIR *dp;
  struct dirent *entry;
  struct stat statbuf;
  if((dp = opendir(dir)) == NULL) {
      fprintf(stderr,"cannot open directory: %s\n", dir);
      return -1;
  }

  chdir(dir);
  while((entry = readdir(dp)) != NULL) {
      lstat(entry->d_name,&statbuf);
      if(S_ISDIR(statbuf.st_mode)) {
          /* Found a directory, but ignore . and .. */
          if(strcmp(".",entry->d_name) == 0 ||
              strcmp("..",entry->d_name) == 0)
              continue;
          if(strstr(entry->d_name, ".aciil_chkpnt-"))
          {
            char * end;
            uint64_t epoch = strtoull(entry->d_name + 14, //14 is strlen of the prefix
                                      &end, 10);
            if(end != entry->d_name + 14 //make sure it is a number
              && epoch > most_recent_epoch // and bigger than the last one
              && epoch < epoch_before) //and smaller than the max
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

int64_t __aciil_get_checkpoint_folder_before(char * checkpoint_dir, char * base_path, int64_t checkpoint_before)
{
  DIR *dp;
  struct dirent *entry;
  struct stat statbuf;
  int64_t most_recent_checkpoint = -1;
  if((dp = opendir(base_path)) == NULL) {
      fprintf(stderr,"cannot open directory: %s\n", base_path);
      return -1;
  }

  chdir(base_path);
  while((entry = readdir(dp)) != NULL) {
      lstat(entry->d_name,&statbuf);
      if(S_ISDIR(statbuf.st_mode)) {
          /* Found a directory, but ignore . and .. */
          if(strcmp(".",entry->d_name) == 0 ||
              strcmp("..",entry->d_name) == 0)
              continue;
          char * end;
          int64_t checkpoint_num = strtoll(entry->d_name, &end, 10);
          if(entry->d_name != end
            && checkpoint_num > most_recent_checkpoint
            && checkpoint_num < checkpoint_before)
          {
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

uint8_t __aciil_checkpoint_valid(int64_t num_variables)
{
  for(int64_t i = 0; i < num_variables; i++)
  {
    char file_name[1024];
    sprintf(file_name, "%"PRIi64, i);
    char * file = __aciil_merge_char_arrays(__aciil_checkpoint_path, file_name);
    FILE *fp;
    fp = fopen(file, "r");
    if(!fp) return 0;
    fclose(fp);
  }
  return 1;
}

int64_t __aciil_restart_get_label()
{
  char base_path[1024];
  char checkpoint_dir[1024];
  uint64_t epoch = UINT64_MAX;
  DIR *dp;
  struct dirent *entry;
  struct stat statbuf;
  int64_t label = -1;
  int64_t num_variables = -1;
  while(1) //todo fix this
  {
    epoch = __aciil_get_checkpoint_before(base_path, epoch);
    if(epoch == 0) return -1; //no checkpoints to use
    printf("Most recent epoch is %s\n", base_path);

    //now that the most recent application run was found
    //find the most recent checkpoint

    int64_t most_recent_checkpoint = INT64_MAX;
    while(1)
    {
      most_recent_checkpoint = __aciil_get_checkpoint_folder_before(checkpoint_dir, base_path, most_recent_checkpoint);
      if(most_recent_checkpoint < 0) break;

      __aciil_checkpoint_path = __aciil_merge_char_arrays(base_path, checkpoint_dir);

      if(!__aciil_checkpoint_path)
      {
        return -1;
      }

      //open the info file which stores the info about the checkpoint
      FILE *fp;
      char * info_file_path = __aciil_merge_char_arrays(__aciil_checkpoint_path, "info");
      if(!info_file_path)
      {
        continue;
      }
      fp = fopen(info_file_path, "r");
      //read the header
      if (fscanf(fp, "%"PRIi64"\n", &label) != 1) //read the label number
      {
        fclose(fp);
        continue;
      }
      if (fscanf(fp, "%"PRIi64"\n", &num_variables) != 1) //read the number of variables
      {
        fclose(fp);
        continue;
      }
      printf("Label is %"PRIi64"\n", label);
      printf("Num variables is %"PRIi64"\n", num_variables);

      fclose(fp);
      free(info_file_path);
      __aciil_restart_checkpoint_file_counter = 0;
      if(label >= 0
        && __aciil_checkpoint_valid(num_variables)) return label;
    }

    if(most_recent_checkpoint < 0) continue;
  }
  return -1;
}

void __aciil_restart_read_from_checkpoint(int64_t sizeBits, char * data)
{
  char file_name[1024];
  sprintf(file_name, "%"PRIi64, __aciil_restart_checkpoint_file_counter);
  char * file = __aciil_merge_char_arrays(__aciil_checkpoint_path, file_name);
  FILE *fp;
  fp = fopen(file, "r");
  //read the header
  int64_t size = 0;
  if (fscanf(fp, "%"PRIi64"\n", &size) != 1) //read size
  {
    printf("Restart has failed - header - aborted.\n");
    exit(-1);
  }

  //read the separator
  if (fscanf(fp, "\n") != 0)
  {
    printf("Restart has failed - separator - aborted.\n");
    exit(-1);
  }

  //read the data
  for(int64_t i = 0; i < ROUND_BITS_TO_BYTES(sizeBits); i++)
  {
    if (fscanf(fp, "%c", &data[i]) != 1) //read char at a time
    {
      printf("Restart has failed - body - aborted.\n");
      exit(-1);
    }
  }

  fclose(fp);
  free(file);
  __aciil_restart_checkpoint_file_counter++;

}
