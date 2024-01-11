#define _DEFAULT_SOURCE
#include "my_malloc.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
MyErrorNo my_errno = MYNOERROR;
#define IDENTIFIER 0x73957623ul
enum { MinimumChunkSize = sizeof(struct freelistnode) };
static FreeListNode head = NULL;
// return ptr that *ptr is the only pointer that points to the chunk found
static FreeListNode *find_fit(size_t size) {
  for (FreeListNode *target = &head; *target != NULL; target = &(*target)->flink) {
    if ((*target)->size >= size + CHUNKHEADERSIZE) {
      return target;
    }
  }
  return NULL;
}
static void insert_chunk(FreeListNode target) {
  FreeListNode *after = &head;
  while (*after != NULL && *after < target) {
    after = &(*after)->flink;
  }
  target->flink = *after;
  *after = target;
}
void *my_malloc(size_t size) {
  // padding
  if ((size & 0x7) != 0) {
    size = (size | 0x7) + 1;
  }
  if (size + CHUNKHEADERSIZE < MinimumChunkSize) {
    size = MinimumChunkSize - CHUNKHEADERSIZE;
  }
  FreeListNode *target = find_fit(size);
  if (target == NULL) {
    FreeListNode new_chunk = sbrk(0);
    intptr_t delta = size + CHUNKHEADERSIZE > 8192 ? size + CHUNKHEADERSIZE : 8192;
    if (sbrk(delta) == (void *)-1) {
      my_errno = MYENOMEM;
      return NULL;
    }
    new_chunk->size = delta;
    insert_chunk(new_chunk);
    target = find_fit(size);
    assert(target != NULL);
  }
  FreeListNode chunk = *target;
  *target = (*target)->flink;
  if (chunk->size - size - CHUNKHEADERSIZE > MinimumChunkSize) {
    FreeListNode split = ((void *)chunk) + size + CHUNKHEADERSIZE;
    split->size = chunk->size - size - CHUNKHEADERSIZE;
    insert_chunk(split);
    chunk->size = size + CHUNKHEADERSIZE;
  }
  uint64_t header = ((chunk->size & 0xffffffffull) << 32) | IDENTIFIER;
  *(uint64_t *)chunk = header;
  void *result = ((void *)chunk) + CHUNKHEADERSIZE;
  memset(result, 0xac, size);
  return result;
}
void my_free(void *ptr) {
  if (ptr == NULL) {
    my_errno = MYBADFREEPTR;
    return;
  }
  void *target = ptr - CHUNKHEADERSIZE;
  uint64_t header = *(uint64_t *)target;
  if ((header & 0xffffffffull) != IDENTIFIER) {
    my_errno = MYBADFREEPTR;
    return;
  }
  FreeListNode chunk = target;
  memset(target, 0, header >> 32);
  chunk->size = header >> 32;
  insert_chunk(chunk);
}
FreeListNode free_list_begin(void) { return head; }
void coalesce_free_list(void) {
  FreeListNode target = head;
  while (target != NULL && target->flink != NULL) {
    void *address = target;
    if (address + target->size == target->flink) {
      target->size += target->flink->size;
      target->flink = target->flink->flink;
    } else {
      target = target->flink;
    }
  }
}