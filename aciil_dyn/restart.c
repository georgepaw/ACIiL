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

int64_t __aciil_restart_get_label()
{
  char base_path[1024];

  char * dir = ".";
  int64_t label = -1;
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
              && epoch > most_recent_epoch) // and bigger than the last one
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
  if(most_recent_epoch == 0) return -1;
  printf("Most recent epoch is %s\n", base_path);

  //now that the most recent application run was found
  //find the most recent checkpoint

  int64_t most_recent_checkpoint = -1;
  char checkpoint_dir[1024];

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
            && checkpoint_num > most_recent_checkpoint)
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
  if(most_recent_checkpoint < 0) return -1;
  printf("Most recent dir is %s\n", checkpoint_dir);

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
    return -1;
  }
  fp = fopen(info_file_path, "r");
  //read the header
  if (fscanf(fp, "%"PRIi64"\n", &label) != 1) //read the label number
  {
    return -1;
  }
  printf("Label is %"PRIi64"\n", label);
  fclose(fp);
  free(info_file_path);
  __aciil_restart_checkpoint_file_counter = 0;
  return label < 0 ? -1 : label;
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
