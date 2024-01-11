#define _DEFAULT_SOURCE
#include "defs.h"
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
int running = 1;
int N;
sem_t available;
sem_t empty;
void shutdown_procedure(struct control_block *control_block, struct process_line *line) {
  sem_wait(&control_block->mutex);
  control_block->total_found += line->found;
  control_block->total_skipped += line->skipped;
  control_block->total_tested += line->tested;
  memset(line, 0, sizeof(struct process_line));
  sem_post(&control_block->mutex);
}
void handle(int signal) {
  (void)signal;
  running = 0;
}
struct arguments {
  mqd_t queue;
  struct control_block *control_block;
  struct process_line *line;
};
void *compute(void *arguments_) {
  struct message message;
  struct arguments *arguments = arguments_;
  mqd_t queue = arguments->queue;
  struct process_line *line = arguments->line;
  while (running) {
    if (sem_wait(&available) == -1) {
      continue;
    }
    int current = N;
    sem_post(&empty);
    int result = 0;
    for (int i = 1; i < current; i++) {
      if (current % i == 0) {
        result += i;
        if (result > current) {
          break;
        }
      }
    }
    if (result != current) {
      continue;
    }
    line->found += 1;
    message.type = MESSAGE_REPORT;
    message.result = current;
    mq_send(queue, (void *)&message, sizeof(message), 0);
  }
  return 0;
}
void search(int start, struct control_block *control_block, struct process_line *line) {
  int current = start;
  do {
    do {
      uint32_t index = current - 1;
      uint8_t mask = 1 << (index & 7);
      sem_wait(&control_block->mutex);
      int flag = control_block->bitmap[index >> 3] & mask;
      control_block->bitmap[index >> 3] |= mask;
      sem_post(&control_block->mutex);
      if (flag) {
        line->skipped += 1;
        break;
      }
      line->tested += 1;
      sem_wait(&empty);
      N = current;
      sem_post(&available);
    } while (0);
    current++;
    if (current > WrapBackBoundary) {
      current = 1;
    }
  } while (current != start && running == 1);
}
int main(int argc, char *argv[]) {
  if (argc != 2) {
    return 1;
  }
  int start = strtol(argv[1], NULL, 10);
  if (start <= 0) {
    return 1;
  }
  if (start > WrapBackBoundary) {
    start = start % WrapBackBoundary + 1;
  }

  mqd_t queue = mq_open(IPC_KEY, O_WRONLY);
  if (queue == -1) {
    return 1;
  }
  int memory = shm_open(IPC_KEY, O_RDWR, 0600);
  if (memory == -1) {
    return 1;
  }
  struct control_block *control_block =
      mmap(NULL, sizeof(struct control_block), PROT_READ | PROT_WRITE, MAP_SHARED, memory, 0);
  if (control_block == NULL) {
    return 1;
  }
  close(memory);
  sem_t *semaphore = sem_open(IPC_KEY, O_RDWR);
  if (semaphore == SEM_FAILED) {
    return 1;
  }

  struct sigaction action = {.sa_handler = handle, .sa_flags = 0};
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGQUIT, &action, NULL);
  sigaction(SIGHUP, &action, NULL);

  struct message message;
  message.type = MESSAGE_REGISTER;
  message.pid = getpid();
  mq_send(queue, (void *)&message, sizeof(message), 0);
  sem_wait(semaphore);
  struct process_line *line = control_block->processes;
  while (line->pid != message.pid) {
    line++;
  }

  sem_init(&available, 1, 0);
  sem_init(&empty, 1, 1);

  pthread_t threads[5];
  struct arguments arguments = {.queue = queue, .control_block = control_block, .line = line};
  for (int i = 0; i < 5; i++) {
    pthread_create(&threads[i], NULL, compute, &arguments);
  }
  search(start, control_block, line);
  N = 1;
  for (int i = 0; i < 5; i++) {
    sem_post(&available);
  }
  for (int i = 0; i < 5; i++) {
    pthread_join(threads[i], NULL);
  }
  sem_destroy(&available);
  sem_destroy(&empty);

  shutdown_procedure(control_block, line);
  if (running) {
    execl("./report", "report", "-k", NULL);
  }
  return 0;
}