/**
 * @file   tm.cpp
 * 
 * @author Matteo Suez <matteo.suez@epfl.ch>
 * 
 * @section DESCRIPTION
 * 
 * Implementation of my transaction manager based on TL2.
 * 
 * The implementation uses some heuristic, such as the alignment size
 * and the workload to preallocate the segments in a static way. By using the stack 
 * instead of the heap it allows improved performance.
**/

// Requested features
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifdef __STDC_NO_ATOMICS__
    #error Current C11 compiler does not support atomic operations
#endif

// Optimizations
#pragma GCC optimize("Ofast")
#pragma GCC target("avx,avx2,fma,bmi,bmi2,popcnt")

/** Define a proposition as likely true.
 * @param prop Proposition
**/
#undef likely
#ifdef __GNUC__
    #define likely(prop) \
        __builtin_expect((prop) ? true : false, true /* likely */)
#else
    #define likely(prop) \
        (prop)
#endif

/** Define a proposition as likely false.
 * @param prop Proposition
**/
#undef unlikely
#ifdef __GNUC__
    #define unlikely(prop) \
        __builtin_expect((prop) ? true : false, false /* unlikely */)
#else
    #define unlikely(prop) \
        (prop)
#endif

/** Define a variable as unused.
**/
#undef unused
#ifdef __GNUC__
    #define unused(variable) \
        variable __attribute__((unused))
#else
    #define unused(variable)
    #warning This compiler has no support for GCC attributes
#endif

// Macros

/** Get the index of segment from virtual address
 *  It uses the first MSB 16 bits of the address
 * @param x virtual memory address
**/
#define indexOf(x) ((((unsigned long) x >> 48) & 0xFFFF) - 1)

/** Get the offset within a segment from virtual address
 *  It uses the last LSB 48 bits of the address
 * @param x virtual memory address
**/
#define offsetOf(x) (((unsigned long) x & 0xFFFFFFFFFFFF) >> 3)

/** Get the virtual address given the segment index
 *  It uses the last LSB 48 bits of the address
 * @param x virtual memory address
**/
#define virtualAddress(x) (void*) ((unsigned long) (x + 1) << 48)

/** Get the pointer to the word given the virtual address
 * @param x virtual memory address
**/
#define findW(x) (&region.memory[indexOf(x)][offsetOf(x)])

/** Remove the lock
 * @param lock pointer to an atomic integer, where LSB is the lock bit
**/
#define unlock(lock) atomic_fetch_sub(lock, 1)

/** Remove the lock and change the version
 * @param lock pointer to an atomic integer, where LSB is the lock bit
 * @param version new version to set
**/
#define update(lock, version) atomic_store(lock, version << 1)

/** Write 64 bits
 * @param x pointer to where write in memory
 * @param i offset
 */
#define EXECUTE(x, i) *((uint64_t*) x + i)

// Handy defines to avoid code repetition
#define ABORT clean(); return false;
#define COMMIT clean(); return true;
#define PRE_VALIDATE Word *word = findW(sourceWord); int before = getVersion(&word->lock);

// Constants

// Virtual start address
#define START (void*) 0x1000000000000

// Number of segments preallocated
#define IDX 128

// Number of words for each segment
#define OFF 1024

// Heuristic about alignment
#define ALIGN_BITS 3
#define ALIGN 8

// Imports

// External headers
#include <atomic>
#include <cstdint>
#include <vector>
#include <unordered_set>
#include <map>

// Internal headers
#include <tm.hpp>

// Namespace convention
using namespace std;

/**
 * @brief Word item contained in a segment of the shared memory region
**/
typedef struct Word {

    // I use a 64 bytes data given a maximum alignment of 8 bytes
    uint64_t data;

    // Versioning lock
    atomic_int lock;

    // Default constructor
    Word() : data(0), lock(0) {}
    Word(const Word& other) : data(other.data), lock(atomic_load(&other.lock)) {}

} Word;

/**
 * @brief Shared Memory Region (a.k.a Transactional Memory).
**/
typedef struct Region {

    // Global version clock
    atomic_int globalClock;
    
    // Counter of allocated segment to manage the virtual address
    atomic_int next;

    // Size of the first segment
    int size;

    // Array of segments, each composed by multiple words
    vector<vector<Word>> memory;

    // Default constructor
    Region() : globalClock(0), next(1), memory(IDX, vector<Word>(OFF)) {}

} Region;

/**
 * @brief Item contained in the write set of a transaction
 */
typedef struct WriteItem {

    // Temporary data storing the info during tm_write
    uint64_t data;

    // Pointer to the associated lock in the read set
    atomic_int *lock;

} WriteItem;

/**
 * @brief Transaction over a shared region
 */
typedef struct Transaction {

    // Read version of the transaction, starting from current value of global clock
    int readVersion;

    // Read set with the pointer to lock
    unordered_set<atomic_int*> readSet;

    // Write set used only by rw transactions, mapping the virtual address to write item
    map<tx_t, WriteItem> writeSet;

    // Read-only flag
    bool readOnly = false;

} Transaction;

// Utilities functions
inline bool rw_read(void const*,size_t,void*);
inline void clean();
inline bool acquire(atomic_int*);
inline int  getVersion(const atomic_int*);

/* Use a static thread-local transaction to avoid useless allocation and reallocation
 * After a transaction is finished, it's simply cleaned instead of being freed
**/
static thread_local Transaction transaction;

/* Use a static region also to avoid the usage of heap
**/
static Region region;

/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/
shared_t tm_create(size_t size, size_t unused(align)) noexcept {
    region.size = size;
    return &region;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 *  Do nothing as it's statically allocated
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t unused(shared)) noexcept {}

/** [thread-safe] Return the start address of the first allocated segment in the shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
**/
void* tm_start(shared_t unused(shared)) noexcept {
    return START;
}

/** [thread-safe] Return the size (in bytes) of the first allocated segment of the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
**/
size_t tm_size(shared_t unused(shared)) noexcept {
    return region.size;
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
**/
size_t tm_align(shared_t unused(shared)) noexcept {
    return ALIGN;
}

/** [thread-safe] Begin a new transaction on the given shared memory region.
 * @param shared Shared memory region to start a transaction on
 * @param is_ro  Whether the transaction is read-only
 * @return Opaque transaction ID, 'invalid_tx' on failure
**/
tx_t tm_begin(shared_t unused(shared), bool is_ro) noexcept {
    transaction.readOnly = is_ro;
    transaction.readVersion = atomic_load(&region.globalClock);
    return (tx_t) &transaction;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t unused(shared), tx_t unused(tx)) noexcept {

    // If the transaction is read only or has nothing on write set to commit
    if (transaction.readOnly || transaction.writeSet.empty()) {  
        COMMIT
    }
    
    // Try to acquire all the locks in the writeset
    for (auto itr1 = transaction.writeSet.begin(); itr1 != transaction.writeSet.end(); ++itr1) {
        if (!acquire(itr1->second.lock)) {
            // Release the acquired locks
            for (auto itr2 = transaction.writeSet.begin(); itr2 != itr1; ++itr2) {
                unlock(itr2->second.lock);
            }
            ABORT 
        }
    }

    // Increment the global version and set the new write version to it
    int writeVersion = atomic_fetch_add(&region.globalClock, 1) + 1;
    
    // If not the special case rv == wv - 1, then validate
    if (transaction.readVersion != writeVersion - 1) {
        for (const auto &item : transaction.readSet) {
            int version = getVersion(item);         
            if (version < 0 || version > transaction.readVersion) {
                // Unlock everything
                for (const auto &entry : transaction.writeSet) {
                    unlock(entry.second.lock);
                }
                ABORT
            }
        }
    } 

    //  Commit the changes: write to memory and update version to wv (as unlocked)
    for (const auto &item : transaction.writeSet) {
        Word *word = findW(item.first);
        word->data = item.second.data;
        update(&word->lock, writeVersion);
    }

    COMMIT
}

/** [thread-safe] Read operation in a read-write transaction, source in the shared region and target in a private region.
 * @param region Shared memory region associated with the transaction
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
**/
inline bool rw_read(void const* source, size_t size, void* target) {

    for (tx_t i = 0; i < size; i += ALIGN) {
        tx_t sourceWord = (tx_t) source + i;

        // Search if already present in the write set, if so just use that value
        auto search = transaction.writeSet.find(sourceWord);
        if (search != transaction.writeSet.end()) {
            EXECUTE(target, i) = search->second.data;
            continue;
        }

        PRE_VALIDATE

        // If the version has already been locked or it's greater than rv
        if (before < 0 || before > transaction.readVersion) {
            ABORT
        }

        // Execute the transaction
        EXECUTE(target, i) = word->data;

        // Sample the lock again to check if a concurrent transaction has occurred
        // If the word has been locked after, or the 2 version numbers are different (or greater than readVersion)
        if (before != getVersion(&word->lock)) {
            ABORT
        }

        // Put in the read set the reference to the lock
        transaction.readSet.emplace(&word->lock);
    }

    return true;
}

/** [thread-safe] Read operation in the READ-ONLY transaction, source in the shared region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction READ-ONLY to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
**/
bool tm_read(shared_t unused(shared), tx_t unused(tx), void const* source, size_t size, void* target) noexcept {

    if (!transaction.readOnly) {
        return rw_read(source, size, target);
    }

    for (tx_t i = 0; i < size; i += ALIGN) {
        tx_t sourceWord = (tx_t) source + i;

        PRE_VALIDATE
        
        // If version lock already taken
        if (before < 0 || before > transaction.readVersion) {
            ABORT
        }

        // Execute transaction
        EXECUTE(target, i) = word->data;

        // Sample the lock again to check if a concurrent transaction has occurred
        int after = getVersion(&word->lock);

        // If the word has been locked after, or the 2 version numbers are different (or greater than readVersion)
        if (before != after) {
            ABORT
        }

    }

    return true;
}

/** [thread-safe] Write operation in the given transaction, source in a private region and target in the shared region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in a private region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in the shared region)
 * @return Whether the whole transaction can continue
**/
bool tm_write(shared_t unused(shared), tx_t unused(tx), void const* source, size_t size, void* target) noexcept {

    for (tx_t i = 0; i < size; i += ALIGN) {
        tx_t destWord = (tx_t) target + i;
        // Simply put in the write set the content of source
        transaction.writeSet[destWord] = {EXECUTE(source, i), &findW(destWord)->lock};
    }

    return true;
}

/** [thread-safe] Memory allocation in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param size   Allocation requested size (in bytes), must be a positive multiple of the alignment
 * @param target Pointer in private memory receiving the address of the first byte of the newly allocated, aligned segment
 * @return Whether the whole transaction can continue (success/nomem), or not (abort_alloc)
**/
Alloc tm_alloc(shared_t unused(shared), tx_t unused(tx), size_t unused(size), void** target) noexcept {
    // Find a new virtual address for the new segment
    *target = virtualAddress(atomic_fetch_add(&region.next, 1));
    return Alloc::success;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t unused(shared), tx_t unused(tx), void* unused(target)) noexcept {
    // Nothing dinamically allocated in a transaction, no need to free
    return true;
}


/**************************************
 *     *  UTILITIES  FUNCTIONS  *     *
 **************************************/


/** Clean the transaction by clearing
 *  write and read sets
 */
inline void clean() {
    if (!transaction.readOnly) {
        transaction.writeSet.clear();
    }
    transaction.readSet.clear();
}

/** Acquire lock
 * @param lock pointer to the version lock
 * @returns true if succeeded to lock, false if locked
 */
inline bool acquire(atomic_int *lock) {
    int version = atomic_load(lock);
    // Sampling the LSB to see if it's locked    
    if ((version & 0x1) > 0) {
        return false;
    }
    // Perform CAS to prevent concurrent lock acquires
    return atomic_compare_exchange_weak(lock, &version, version | 0x1);
}

/** Get the version of the lock (exclude LSB)
 * @param lock ointer to the version lock
 * @returns -1 if it's locked, version number otherwise
 */
inline int getVersion(const atomic_int *lock) {
    int version = atomic_load(lock);
    // If it's locked return -1
    if ((version & 0x1) > 0) {
        return -1;
    }
    return version >> 1;
}