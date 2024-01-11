#define _DEFAULT_SOURCE
#include "defs.h"
#include <fcntl.h>
#include <mqueue.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
int running = 1;
void ipc_cleanup() {
  mq_unlink(IPC_KEY);
  shm_unlink(IPC_KEY);
  sem_unlink(IPC_KEY);
}
void shutdown_procedure(struct control_block *control_block, int status) {
  for (size_t i = 0; i < sizeof(control_block->processes) / sizeof(struct process_line); i++) {
    if (control_block->processes[i].pid != 0) {
      kill(control_block->processes[i].pid, SIGINT);
    }
  }
  sleep(5);
  sem_destroy(&control_block->mutex);
  exit(status);
}
void handle(int signal) {
  (void)signal;
  running = 0;
}
int main() {
  // 1. setup
  // 1.0. ipc cleaner
  atexit(ipc_cleanup);
  // 1.1. message queue
  struct mq_attr attribution = {.mq_msgsize = sizeof(struct message), .mq_maxmsg = 5};
  mqd_t queue = mq_open(IPC_KEY, O_RDONLY | O_CREAT | O_EXCL, 0600, &attribution);
  if (queue == -1) {
    return 1;
  }
  // 1.2. shared memory
  int memory = shm_open(IPC_KEY, O_RDWR | O_CREAT | O_EXCL, 0600);
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
  memset(control_block, 0, sizeof(struct control_block));
  sem_init(&control_block->mutex, 1, 1);
  control_block->manager = getpid();
  // 1.3. semaphore
  sem_t *semaphore = sem_open(IPC_KEY, O_RDWR | O_CREAT | O_EXCL, 0600, 0);
  if (semaphore == SEM_FAILED) {
    shutdown_procedure(control_block, 1);
  }
  // 1.4. signal handler
  struct sigaction action = {.sa_handler = handle, .sa_flags = 0};
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGQUIT, &action, NULL);
  sigaction(SIGHUP, &action, NULL);

  struct message message;
  size_t perfect_numbers = 0;

  while (running) {
    ssize_t mq_result = mq_receive(queue, (char *)&message, sizeof(message), NULL);
    if (mq_result != sizeof(message)) {
      continue;
    }
    if (message.type == MESSAGE_REGISTER) {
      sem_wait(&control_block->mutex);
      size_t i;
      for (i = 0; i < sizeof(control_block->processes) / sizeof(struct process_line); i++) {
        if (control_block->processes[i].pid == 0) {
          break;
        }
      }
      sem_post(&control_block->mutex);
      control_block->processes[i].pid = message.pid;
      sem_post(semaphore);
    } else if (message.type == MESSAGE_REPORT) {
      size_t target = 0;
      for (target = 0; target != perfect_numbers; target++) {
        if (control_block->perfect_numbers[target] > message.result) {
          break;
        }
      }
      for (size_t i = perfect_numbers; i > target; i--) {
        control_block->perfect_numbers[i] = control_block->perfect_numbers[i - 1];
      }
      control_block->perfect_numbers[target] = message.result;
      perfect_numbers++;
    } else {
      break;
    }
  }
  shutdown_procedure(control_block, 0);
}