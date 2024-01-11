#ifndef DEFS_H_
#define DEFS_H_
#include <semaphore.h>
#include <stdint.h>
#include <sys/types.h>
#define IPC_KEY "/YOUR_PHONE_NUMBER_HERE"
struct control_block {
  sem_t mutex;
  uint8_t bitmap[0x400000];
  uint32_t perfect_numbers[20];
  struct process_line {
    pid_t pid;
    uint32_t tested;
    uint32_t skipped;
    uint32_t found;
  } processes[20];
  pid_t manager;
  uint32_t total_tested;
  uint32_t total_skipped;
  uint32_t total_found;
};
struct message {
  enum message_type {
    MESSAGE_REGISTER,
    MESSAGE_REPORT,
  } type;
  union {
    pid_t pid;
    uint32_t result;
  };
};
enum { WrapBackBoundary = sizeof(((struct control_block *)0)->bitmap) * 8 };
#endif