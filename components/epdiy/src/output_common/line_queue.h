#pragma once

#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>


/// Circular line queue with atomic read / write operations.
typedef struct {
    int size;
    atomic_int current;
    atomic_int last;
    //uint8_t* buf;
    uint8_t** bufs;
    size_t element_size;
} LineQueue_t;

LineQueue_t lq_init(int queue_len);
void lq_free(LineQueue_t* queue);

/// Pointer to the next empty element in the line queue.
///
/// NULL if the queue is currently full.
uint8_t* lq_current(LineQueue_t* queue);

/// Advance the line queue.
void lq_commit(LineQueue_t* queue);

/// Read from the line queue.
///
/// Returns 0 for a successful read to `dst`, -1 for a failed read (empty queue).
int lq_read(LineQueue_t* queue, uint8_t* dst);

/// Reset the queue into an empty state.
/// This operation is *not* atomic!
void lq_reset(LineQueue_t* queue);
