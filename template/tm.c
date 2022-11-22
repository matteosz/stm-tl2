/**
 * @file   tm.c
 * @author [...]
 *
 * @section LICENSE
 *
 * [...]
 *
 * @section DESCRIPTION
 *
 * Implementation of your own transaction manager.
 * You can completely rewrite this file (and create more files) as you wish.
 * Only the interface (i.e. exported symbols and semantic) must be preserved.
**/

// Requested features
#define _GNU_SOURCE
#define _POSIX_C_SOURCE   200809L
#ifdef __STDC_NO_ATOMICS__
    #error Current C11 compiler does not support atomic operations
#endif

// External headers
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Internal headers
#include <tm.h>

#include "macros.h"

static const tx_t read_only_tx  = UINTPTR_MAX - 10;
static const tx_t read_write_tx = UINTPTR_MAX - 11;

enum type {READ,WRITE};

struct version_clock_struct {
    u_int64_t version;
    bool locked;
};
typedef struct version_clock_struct* version_clock;

typedef struct read_node read_node;

struct read_node {
    void* address;
    read_node* prev;
    read_node* next;
};

typedef struct write_node write_node;

struct write_node {
    void* address;
    void* value;
    write_node* prev;
    write_node* next;
};

/**
 * @brief List of dynamically allocated segments.
 */
struct segment_node {
    struct segment_node* prev;
    struct segment_node* next;
    u_int64_t rv;
    read_node* r_head;
    write_node* w_head;
};
typedef struct segment_node* segment_list;

/**
 * @brief Simple Shared Memory Region (a.k.a Transactional Memory).
 */
struct region {
    void* start;        // Start of the shared memory region (i.e., of the non-deallocable memory segment)
    segment_list segments; // Shared memory segments dynamically allocated via tm_alloc within transactions
    size_t size;        // Size of the non-deallocable memory segment (in bytes)
    size_t align;       // Size of a word in the shared memory region (in bytes)
    version_clock global_version;
};

static inline bool c&s(void* ptr, bool old, bool new) {
    return __sync_val_compare_and_swap(ptr, old, new);
}

static inline segment_list segment_create(segment_list prev, segment_list next) {
    segment_list segment = (segment_list) malloc(sizeof(struct struct segment_node));
    segment->prev = prev;
    segment->next = next;
    segment->rv = 0;
    segment->r_head = NULL;
    segment->w_head = NULL;
    return segment;
}

static inline void r_set_destroy(read_node* node) {
    while (node) {
        read_node* tmp = node;
        free(tmp);
        node = node->next;
    }
}

static inline void w_set_destroy(write_node* node) {
    while (node) {
        write_node* tmp = node;
        free(tmp);
        node = node->next;
    }
}

static inline void segments_destroy(segment_list segment) {
    while (segment) {
        w_set_destroy(segment->w_head);
        r_set_destroy(segment->r_head);
        segment_list tmp = segment;
        free(tmp);
        segment = segment->next;
    }
}

static inline bool global_version_clock_acquire(bool* lock) {
    return c&s(lock, false, true);
}

static inline bool global_version_clock_release(bool* lock) {
    return c&s(lock, true, false);
}

/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/
shared_t tm_create(size_t size, size_t align) {
    struct region* region = (struct region*) malloc(sizeof(struct region));
    if (unlikely(!region)) {
        return invalid_shared;
    }

    if (posix_memalign(&(region->start), align, size) != 0) {
        free(region);
        return invalid_shared;
    }

    memset(region->start, 0, size);
    region->segments    = NULL;
    region->size        = size;
    region->align       = align;
    region->global_version = (version_clock) malloc(sizeof(struct version_clock_struct));
    if (unlikely(!region->global_version)) {
        return invalid_shared;
    }
    region->global_version->locked = false;
    region->global_version->version = 0;
    return region;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t shared) {
    struct region* region = (struct region*) shared;
    segments_destroy(region->segments);
    free(region->start);
    free(region->global_version);
    free(region);
}

/** [thread-safe] Return the start address of the first allocated segment in the shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
**/
void* tm_start(shared_t shared) {
    return ((struct region*) shared)->start;
}

/** [thread-safe] Return the size (in bytes) of the first allocated segment of the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
**/
size_t tm_size(shared_t shared) {
    return ((struct region*) shared)->size;
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
**/
size_t tm_align(shared_t shared) {
    return ((struct region*) shared)->align;
}

/** [thread-safe] Begin a new transaction on the given shared memory region.
 * @param shared Shared memory region to start a transaction on
 * @param is_ro  Whether the transaction is read-only
 * @return Opaque transaction ID, 'invalid_tx' on failure
**/
tx_t tm_begin(shared_t unused(shared), bool unused(is_ro)) {
    // TODO: tm_begin(shared_t)
    return invalid_tx;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t unused(shared), tx_t unused(tx)) {
    // TODO: tm_end(shared_t, tx_t)
    return false;
}

/** [thread-safe] Read operation in the given transaction, source in the shared region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
**/
bool tm_read(shared_t unused(shared), tx_t unused(tx), void const* unused(source), size_t unused(size), void* unused(target)) {
    // TODO: tm_read(shared_t, tx_t, void const*, size_t, void*)
    return false;
}

/** [thread-safe] Write operation in the given transaction, source in a private region and target in the shared region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in a private region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in the shared region)
 * @return Whether the whole transaction can continue
**/
bool tm_write(shared_t unused(shared), tx_t unused(tx), void const* unused(source), size_t unused(size), void* unused(target)) {
    // TODO: tm_write(shared_t, tx_t, void const*, size_t, void*)
    return false;
}

/** [thread-safe] Memory allocation in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param size   Allocation requested size (in bytes), must be a positive multiple of the alignment
 * @param target Pointer in private memory receiving the address of the first byte of the newly allocated, aligned segment
 * @return Whether the whole transaction can continue (success/nomem), or not (abort_alloc)
**/
alloc_t tm_alloc(shared_t unused(shared), tx_t unused(tx), size_t unused(size), void** unused(target)) {
    // TODO: tm_alloc(shared_t, tx_t, size_t, void**)
    return abort_alloc;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t unused(shared), tx_t unused(tx), void* unused(target)) {
    // TODO: tm_free(shared_t, tx_t, void*)
    return false;
}
