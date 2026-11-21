#define _DEFAULT_SOURCE 1
#include "alloc.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>

// Set beginning and end of program break
void *orig_break;
void *current_break;
/*
__attribute__((constructor)) void original_program_break() {
  orig_break = sbrk(0);
  current_break = orig_break;
}
*/

extern char end;
// Create block for current program break
// Create options for allocator (first fit is default)
enum algs alloc_algorithm = FIRST_FIT;
int limit; // not sure what this does yet

// flag to mark first run
int first_run = 0;

struct header *first_block;
struct header *first_alloc_block;
// Allocation algorithms
// TODO: actually write them
struct header *find_first_fit_block(size_t size);
struct header *find_best_fit_block(size_t size);
struct header *find_worst_fit_block(size_t size);
void coalesce();
void *expand_heap();

void allocopt(enum algs alg, int new_limit) {
  alloc_algorithm = alg;
  limit = new_limit;
  orig_break = sbrk(0);
  if (orig_break < current_break) {
    memset(orig_break, '\0', current_break - orig_break);
    // Reset current break to original program break;
    current_break = orig_break;
    brk(orig_break);
  }
  /*
  orig_break = sbrk(INCREMENT);
  current_break = sbrk(0);
  first_block = orig_break;
  first_block->size = sbrk(0) - orig_break;
  first_block->next = NULL;
  */
}

void *alloc(int size) {
  // post allocopt()
  if (current_break == orig_break) {
    orig_break = sbrk(INCREMENT);
    current_break = sbrk(0);
    first_block = orig_break;
    first_block->size = sbrk(0) - orig_break;
    first_block->next = NULL;
  }
  // If break is full
  else if (first_block == NULL) {
    expand_heap();
    if (first_block == NULL)
      return NULL;
  }
  // Get current program break
  struct header *block;
  switch (alloc_algorithm) {
  case FIRST_FIT: {
    block = find_first_fit_block(size);
    break;
  }
  case BEST_FIT: {
    block = find_best_fit_block(size);
    break;
  }
  case WORST_FIT: {
    block = find_worst_fit_block(size);
    break;
  }
  }
  if (block != NULL) {
    // Create new free block
    struct header *new_free_block = NULL;
    if (block->size - (sizeof(struct header) + size) > sizeof(struct header)) {
      new_free_block = (void *)block + sizeof(struct header) + size;
      // Subtract allocated region from new size (includes block header)
      new_free_block->size = block->size - size - sizeof(struct header);
      // update block size
      block->size = sizeof(struct header) + size;
      // Add rest of linked list to new free block
      new_free_block->next = first_block;
      first_block = new_free_block;
    }
    // Cycle to find old free block
    struct header *prev_block = first_block;
    struct header *current_block = first_block;
    while (current_block != block) {
      prev_block = current_block;
      current_block = current_block->next;
    }
    // Remove old block from list
    if (new_free_block == NULL && first_block->next == NULL) {
      first_block = NULL;
    } else if (first_block == block) {
      first_block = block->next;
    } else {
      prev_block->next = block->next;
    }
    void *pointer = block;

    return pointer + sizeof(struct header);
  } else {
    return NULL;
  }
}

void dealloc(void *loc) {
  struct header *block = loc - sizeof(struct header);
  memset(loc, '\0', block->size - sizeof(struct header));
  block->next = first_block;
  first_block = block;
  coalesce();
}

struct allocinfo allocinfo(void) {
  struct allocinfo info;
  // Initialize statistics
  // TODO: check if free chunk sizes should account for header
  // Traverse linked list of blocks and update statistics
  struct header *current_block = first_block;
  info.free_size = 0;
  info.free_chunks = 0;
  info.smallest_free_chunk_size = 0;
  info.largest_free_chunk_size = 0;
  while (current_block != NULL) {
    if (info.smallest_free_chunk_size == 0) {
      info.smallest_free_chunk_size = current_block->size;
    }
    if (info.largest_free_chunk_size == 0) {
      info.largest_free_chunk_size = current_block->size;
    }
    info.free_size += current_block->size - sizeof(struct header);
    info.free_chunks++;
    if (info.largest_free_chunk_size < current_block->size) {
      info.largest_free_chunk_size = current_block->size;
    } else if (info.smallest_free_chunk_size > current_block->size) {
      info.smallest_free_chunk_size = current_block->size;
    }
    current_block = current_block->next;
  }
  return info;
}

// Coalesce adjacent free blocks together
void coalesce() {
  struct header *prev_block = first_block;
  if (first_block != NULL && first_block->next != NULL) {
    struct header *current_block = first_block->next;
    while (current_block != NULL) {
      // Check if current block comes after first block
      if ((void *)first_block + first_block->size == (void *)current_block) {
        // Set block size to include new memory area
        first_block->size += current_block->size;
        // Remove old block from linked list
        prev_block->next = current_block->next;
        // Clear current block header
        memset(current_block, '\0', sizeof(struct header));
        // Set one block back for iteration
        current_block = prev_block;
        // Check if current block comes before first block
      } else if ((void *)current_block + current_block->size ==
                 (void *)first_block) {
        current_block->size += first_block->size;
        prev_block->next = current_block->next;
        // Prepare current block to be first block
        current_block->next = first_block->next;
        void *temp = first_block;
        first_block = current_block;
        memset(temp, '\0', sizeof(struct header));
      }
      prev_block = current_block;
      current_block = current_block->next;
    }
  }
}

// Expands heap and returns new block.
void *expand_heap() {
  // Check if heap will go over limit
  if (current_break - orig_break + INCREMENT <= limit) {
    void *old_break = sbrk(INCREMENT);
    if (old_break == (void *)-1 && errno == ENOMEM) {
      return NULL;
    }
    current_break = sbrk(0);
    struct header *new_block = old_break;
    new_block->size = current_break - old_break;
    new_block->next = first_block;
    first_block = new_block;
    coalesce();
    // Bring back last block
    return first_block;
  } else {
    return NULL;
  }
}

// Allocation methods
struct header *find_first_fit_block(size_t size) {
  struct header *current_block = first_block;
  int block_found = 0;
  while (current_block != NULL && block_found == 0) {
    if (current_block->size - sizeof(struct header) >= size) {
      return current_block;
    }
    current_block = current_block->next;
  }

  if (block_found == 0) {
    void *expand_success = expand_heap();
    if (expand_success == NULL)
      return NULL;
    else
      return find_first_fit_block(size);
  }
  return current_block;
}

struct header *find_best_fit_block(size_t size) {
  struct header *current_block = first_block;
  struct header *best_block = NULL;
  while (current_block != NULL) {
    if (current_block->size - sizeof(struct header) >= size) {
      if (best_block == NULL || current_block->size < best_block->size)
        best_block = current_block;
    }
    current_block = current_block->next;
  }
  if (best_block == NULL) {
    void *expand_success = expand_heap();
    if (expand_success == NULL)
      return NULL;
    else
      return find_best_fit_block(size);
  }
  return best_block;
}

struct header *find_worst_fit_block(size_t size) {
  struct header *current_block = first_block;
  struct header *worst_block = NULL;
  while (current_block != NULL) {
    if (current_block->size - sizeof(struct header) >= size) {
      if (worst_block == NULL || current_block->size > worst_block->size)
        worst_block = current_block;
    }
    current_block = current_block->next;
  }
  if (worst_block == NULL) {
    void *expand_success = expand_heap();
    if (expand_success == NULL)
      return NULL;
    else
      return find_best_fit_block(size);
  }
  return worst_block;
}
