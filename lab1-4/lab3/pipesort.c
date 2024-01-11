#define _POSIX_SOURCE
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
void fetch_word(char **destination, size_t capability, FILE *source) {
  if (*destination == NULL) {
    return;
  }
  if (fgets(*destination, capability, source) == NULL) {
    goto terminate;
  }
  size_t length = strlen(*destination);
  if (length == 0 || (length == 1 && (*destination)[length - 1] == '\n')) {
    goto terminate;
  }
  if ((*destination)[length - 1] == '\n') {
    (*destination)[length - 1] = '\0';
  }
  return;
terminate:
  free(*destination);
  *destination = NULL;
  return;
}
int main(int argc, char *argv[]) {
  unsigned int sorters = 1;
  size_t less = 0;
  size_t more = -1;
  if (argc > 1) {
    ++argv;
    while (argv[0] != NULL) {
      if (argv[0][0] != '-') {
        return 1;
      }
      if (argv[0][1] == 'n') {
        if (argv[0][2] == '\0') {
          sorters = strtoul(argv[1], NULL, 10);
          argv++;
        } else {
          sorters = strtoul(argv[0] + 2, NULL, 10);
        }
        argv++;
      } else if (argv[0][1] == 's') {
        if (argv[0][2] == '\0') {
          less = strtoul(argv[1], NULL, 10);
          argv++;
        } else {
          less = strtoul(argv[0] + 2, NULL, 10);
        }
        argv++;
      } else if (argv[0][1] == 'l') {
        if (argv[0][2] == '\0') {
          more = strtoul(argv[1], NULL, 10);
          argv++;
        } else {
          more = strtoul(argv[0] + 2, NULL, 10);
        }
        argv++;
      } else {
        return 1;
      }
    }
  }
  pid_t *sorter_pid = malloc(sizeof(pid_t) * sorters);
  FILE **distribute_pipes = malloc(sizeof(FILE *) * sorters);
  FILE **collect_pipes = malloc(sizeof(FILE *) * sorters);
  for (unsigned int i = 0; i < sorters; i++) {
    int result = 0;
    int distribute_pipe[2];
    int collect_pipe[2];
    result |= pipe(distribute_pipe);
    result |= pipe(collect_pipe);
    pid_t pid = fork();
    if (pid == -1) {
      return 1;
    }
    if (pid == 0) {
      for (unsigned int j = 0; j < i; j++) {
        fclose(distribute_pipes[j]);
        fclose(collect_pipes[j]);
      }
      free(sorter_pid);
      free(distribute_pipes);
      free(collect_pipes);
      result |= (dup2(distribute_pipe[0], STDIN_FILENO) == -1);
      result |= (dup2(collect_pipe[1], STDOUT_FILENO) == -1);
      close(distribute_pipe[0]);
      close(distribute_pipe[1]);
      close(collect_pipe[0]);
      close(collect_pipe[1]);
      if (result) {
        return 1;
      }
      execl("/usr/bin/sort", "/usr/bin/sort", NULL);
      return 1;
    }
    sorter_pid[i] = pid;
    distribute_pipes[i] = fdopen(distribute_pipe[1], "w");
    collect_pipes[i] = fdopen(collect_pipe[0], "r");
    close(distribute_pipe[0]);
    close(collect_pipe[1]);
  }
  pid_t pid = fork();
  if (pid == -1) {
    return 1;
  }
  if (pid != 0) {
    // collect
    free(sorter_pid);
    for (unsigned int i = 0; i < sorters; i++) {
      fclose(distribute_pipes[i]);
    }
    free(distribute_pipes);
    size_t maximum_length = 512;
    char **buffers = malloc(sizeof(char *) * sorters);
    for (unsigned int i = 0; i < sorters; i++) {
      buffers[i] = malloc(maximum_length + 2);
    }
    char *next = malloc(maximum_length + 1);
    next[0] = '\0';
    int next_valid = 0;
    size_t next_count = 0;
    for (unsigned int i = 0; i < sorters; i++) {
      fetch_word(buffers + i, maximum_length + 2, collect_pipes[i]);
    }
    int remaining = 1;
    int matched = 0;
    int selecting = 1;
    while (remaining) {
      remaining = 0;
      matched = 0;
      for (unsigned int i = 0; i < sorters; i++) {
        if (buffers[i] == NULL) {
          continue;
        }
        remaining = 1;
        if (selecting) {
          if (next_valid == 0 || strcmp(buffers[i], next) < 0) {
            strcpy(next, buffers[i]);
            next_valid = 1;
          }
        } else {
          if (strcmp(next, buffers[i]) == 0) {
            matched = 1;
            next_count += 1;
            fetch_word(buffers + i, maximum_length + 2, collect_pipes[i]);
          }
        }
      }
      if (selecting) {
        selecting = 0;
        next_count = 0;
      } else {
        if (matched == 0) {
          selecting = 1;
          next_valid = 0;
          assert(next_count >= 1);
          printf("%-10lu%s\n", next_count, next);
        }
      }
    }
    free(buffers);
    free(next);
    free(collect_pipes);
    return 0;
  }
  for (unsigned int i = 0; i < sorters; i++) {
    fclose(collect_pipes[i]);
  }
  free(collect_pipes);
  size_t current_length = 0;
  unsigned int current_sorter = 0;
  char *buffer = NULL;
  if (less != 0) {
    buffer = malloc(less);
  }
  while (!feof(stdin)) {
    char c = getchar();
    if (isalpha(c)) {
      if (current_length >= more) {
        continue;
      }
      if (current_length == less) {
        for (size_t i = 0; i < less; i++) {
          fputc(buffer[i], distribute_pipes[current_sorter]);
        }
      }
      if (current_length >= less) {
        fputc(tolower(c), distribute_pipes[current_sorter]);
      } else {
        buffer[current_length] = tolower(c);
      }
      ++current_length;
    } else {
      if (current_length > less) {
        fputc('\n', distribute_pipes[current_sorter]);
      }
      current_sorter = (current_sorter + 1) % sorters;
      current_length = 0;
    }
  }
  free(buffer);
  free(sorter_pid);
  free(distribute_pipes);
  return 0;
}