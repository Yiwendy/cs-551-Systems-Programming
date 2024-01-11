#define _DEFAULT_SOURCE
#include "defs.h"
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
int main(int argc, char *argv[]) {
  if (argc > 2) {
    return 1;
  }
  if (argc == 2 && strcmp(argv[1], "-k") != 0) {
    return 0;
  }
  int memory = shm_open(IPC_KEY, O_RDWR, 0600);
  if (memory == -1) {
    return 1;
  }
  ftruncate(memory, sizeof(struct control_block));
  struct control_block *control_block =
      mmap(NULL, sizeof(struct control_block), PROT_READ | PROT_WRITE, MAP_SHARED, memory, 0);
  if (control_block == NULL) {
    return 1;
  }
  close(memory);
  sem_wait(&control_block->mutex);
  printf("Perfect Number Found: ");
  for (size_t i = 0; i < sizeof(control_block->perfect_numbers) / sizeof(uint32_t); i++) {
    if (control_block->perfect_numbers[i] == 0) {
      break;
    }
    printf("%u ", control_block->perfect_numbers[i]);
  }
  printf("\n");
  uint32_t cached_found = 0;
  uint32_t cached_tested = 0;
  uint32_t cached_skipped = 0;
  for (size_t i = 0; i < sizeof(control_block->processes) / sizeof(struct process_line); i++) {
    if (control_block->processes[i].pid == 0) {
      continue;
    }
    struct process_line *line = control_block->processes + i;
    printf(
        "pid(%d): found: %u,\ttested: %u,\tskipped: %u\n", line->pid, line->found, line->tested, line->skipped
    );
    cached_found += line->found;
    cached_skipped += line->skipped;
    cached_tested += line->tested;
  }
  printf("Statistics:\n");
  printf("Total found:\t%-7u\n", control_block->total_found + cached_found);
  printf("Total tested:\t%-7u\n", control_block->total_tested + cached_tested);
  printf("Total skipped:\t%-7u\n", control_block->total_skipped + cached_skipped);
  sem_post(&control_block->mutex);
  if (argc == 2) {
    kill(control_block->manager, SIGINT);
  }
  return 0;
}