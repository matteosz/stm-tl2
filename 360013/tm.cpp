/**
 * @file   tm.cpp
 * @author Matteo Suez
 * 
 * @section DESCRIPTION
 * 
 * TL2-based implementation of STM
**/

// Internal headers
#include <transaction.hpp>

// Shorthand macro
#define INIT(x) Segment *segment = region->memory[index(x)];                          \
                size_t off = offset(x);                                               \
                int start = off / region->align, wordNum = size / region->align;

/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/
shared_t tm_create(size_t size, size_t align) noexcept {
    try {
        return new Region(align, size);
    } catch (const bad_alloc& e) {
        return invalid_shared;
    }
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t shared) noexcept {
    REG
    delete region;
}

/** [thread-safe] Return the start address of the first allocated segment in the shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
**/
void* tm_start(shared_t unused(shared)) noexcept {
    #ifdef _DEBUG_
        cout << "TM_START-> address:" << address(0) << "\n";
    #endif
    return address(0);
}

/** [thread-safe] Return the size (in bytes) of the first allocated segment of the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
**/
size_t tm_size(shared_t shared) noexcept {
    REG
    #ifdef _DEBUG_
        cout << "TM_SIZE-> size: " << region->memory[0]->size << "\n";
    #endif
    return region->memory[0]->size;
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
**/
size_t tm_align(shared_t shared) noexcept {
    REG
    #ifdef _DEBUG_
        cout << "TM_ALIGN-> align: " << region->align << "\n";
    #endif
    return region->align;
}

/** [thread-safe] Begin a new transaction on the given shared memory region.
 * @param shared Shared memory region to start a transaction on
 * @param is_ro  Whether the transaction is read-only
 * @return Opaque transaction ID, 'invalid_tx' on failure
**/
tx_t tm_begin(shared_t shared, bool is_ro) noexcept {
    REG
    try {
        return (tx_t) new Transaction(is_ro, region);
    } catch (const bad_alloc& e) {
        #ifdef _DEBUG_
            cout << "TM_BEGIN-> failed to allocate\n";
        #endif
        return invalid_tx;
    }
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t unused(shared), tx_t tx) noexcept {
    TX 
    bool result = transaction->commit();
    #ifdef _DEBUG_
        cout << "Transaction ended, commit: " << result << "\n";
    #endif
    return result;
}

/** [thread-safe] Read operation in the given transaction, source in the shared region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction READ-ONLY to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
**/
bool ro_read(shared_t shared, Transaction *transaction, void const* source, size_t size, void* target) noexcept {
    REG INIT(source)

    #ifdef _DEBUG_
        cout << "TM_READONLY start\n";
    #endif

    // Pre validate the locks
    int before[wordNum];
    for (int idx = 0; idx < wordNum; idx++) {
        before[idx] = segment->locks[start + idx].load();
        if (isLocked(before[idx])) {
            #ifdef _DEBUG_
                cout << "Found a word already locked\n";
            #endif
            ABORT
        }
    }

    #ifdef _DEBUG_
        cout << "Locks were free, copying memory\n";
    #endif

    // Execute the transaction
    memcpy(target, (void*) ((size_t) segment->data + off), size);

    #ifdef _DEBUG_
        cout << "Post validation starts\n";
    #endif

    // Post validate the version
    for (int idx = 0; idx < wordNum; idx++) {
        atomic_int *lock = &segment->locks[start + idx];
        int after = lock->load();

        // Either after has been locked or after is simply different
        if (before[idx] != after) {
            #ifdef _DEBUG_
                cout << "After version (" << after << ") different from before (" << before[idx] << ")\n";
            #endif
            ABORT
        // If after has not been locked then it's safe to cast
        } else if (after > transaction->readVersion) {
            // Try to revalidate
            #ifdef _DEBUG_
                cout << "After version (" << after << ") greater than rv (" << transaction->readVersion << ")\n";
            #endif
            int clock = region->globalClock.load();
            if (!transaction->validate()) {
                #ifdef _DEBUG_
                    cout << "Failed to revalidate\n";
                #endif
                ABORT
            }
            transaction->readVersion = clock;
            #ifdef _DEBUG_
                cout << "Changed the rv to " << clock << "\n";
            #endif
        }

        transaction->readSet.emplace(lock);
    }

    #ifdef _DEBUG_
        cout << "TM_READONLY success\n";
    #endif
    return true;
}

/** [thread-safe] Read operation in the given transaction, source in the shared region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Write transaction to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
**/
bool rw_read(shared_t shared, Transaction *transaction, void const* source, size_t size, void* target) noexcept {
    REG INIT(source)

    #ifdef _DEBUG_
        cout << "TM_READ starts\n";
    #endif

    size_t data = (size_t) segment->data;
    uintptr_t src = (uintptr_t) source, tgt = (uintptr_t) target;

    for (int idx = 0; idx < wordNum; idx++) {
        void *word = (void*) (src + idx * region->align);

        // Speculative execution
        auto search = transaction->writeSet.find(word);
        if (search != transaction->writeSet.end()) {
            #ifdef _DEBUG_
                cout << "Address found on write set, just copying\n";
            #endif
            memcpy(target, search->second, region->align);
            continue;
        }

        #ifdef _DEBUG_
            cout << "Address not found on write set, trying to sample lock...\n";
        #endif

        atomic_int *lock = &segment->locks[start + idx];

        int before = lock->load();

        if (isLocked(before) || (before > transaction->readVersion)) {
            #ifdef _DEBUG_
                cout << "Failed: before= " << before << ", rv= " << transaction->readVersion << "\n";
            #endif
            ABORT
        }

        memcpy((void*) (tgt + idx * region->align), (void*) (data + (start + idx) * region->align), region->align);

        #ifdef _DEBUG_
            cout << "Content copied in memory, trying to resample lock...\n";
        #endif

        // Sample the lock again to check if a concurrent transaction has occurred
        // If the word has been locked after, or the 2 version numbers are different or greater than read version
        if (before != lock->load()) {
            #ifdef _DEBUG_
                cout << "After lock were taken, or different from before\n";
            #endif            
            ABORT
        }

        transaction->readSet.emplace(lock);
    }

    #ifdef _DEBUG_
        cout << "TM_READ success\n";
    #endif

    return true;
}

/** [thread-safe] Read operation in the given transaction, source in the shared region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
**/
bool tm_read(shared_t shared, tx_t tx, void const* source, size_t size, void* target) noexcept {
    TX
    return transaction->readOnly? ro_read(shared, transaction, source, size, target):
                                  rw_read(shared, transaction, source, size, target);
}

/** [thread-safe] Write operation in the given transaction, source in a private region and target in the shared region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in a private region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in the shared region)
 * @return Whether the whole transaction can continue
**/
bool tm_write(shared_t shared, tx_t tx, void const* source, size_t size, void* target) noexcept {
    REG TX INIT(target)

    #ifdef _DEBUG_
        cout << "TM_WRITE start\n";
    #endif

    uintptr_t src = (uintptr_t) source, tgt = (uintptr_t) target;

    for (int idx = 0; idx < wordNum; idx++) {
        atomic_int *lock = &segment->locks[start + idx];

        int before = lock->load();

        if (isLocked(before) || (before > transaction->readVersion)) {
            #ifdef _DEBUG_
                cout << "Before: " << before << ", rv: " << transaction->readVersion << "\n";
            #endif
            ABORT
        }

        void *targetWord = (void*) (tgt + idx * region->align);
        void *sourceWord = (void*) (src + idx * region->align);

        // If already in the write set
        auto search = transaction->writeSet.find(targetWord);
        if (search != transaction->writeSet.end()) {
            #ifdef _DEBUG_
                cout << "Address found on write set\n";
            #endif
            memcpy(search->second, sourceWord, region->align);
            continue;
        }

        void *copy = malloc(region->align);
        if (unlikely(!copy)) {
            #ifdef _DEBUG_
                cout << "Failed to malloc\n";
            #endif
            ABORT
        }

        // Copy the content of the source to a temporary memory region
        memcpy(copy, sourceWord, region->align);

        #ifdef _DEBUG_
            cout << "Content copied in memory, putting into writeset...\n";
        #endif

        // Insert that address into the writeSet
        transaction->writeSet[targetWord] = copy;

        // Remove from read set to avoid problematic duplicates when validating
        transaction->readSet.erase(lock);
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
Alloc tm_alloc(shared_t shared, tx_t unused(tx), size_t size, void** target) noexcept {
    REG
    #ifdef _DEBUG_
        cout << "TM_ALLOC starts\n";
    #endif
    int count = region->getCount();
    try {
        #ifdef _DEBUG_
            cout << "Trying to alloc the new segment on count id\n";
        #endif
        region->memory[count] = new Segment(region->align, size);
    } catch(const std::runtime_error& e) {
        #ifdef _DEBUG_
            cout << "Alloc failed\n";
        #endif
        region->missingIdx.push(count);
        return Alloc::nomem;
    }
    #ifdef _DEBUG_
        cout << "Alloc successed\n";
    #endif
    *target = address(count);
    return Alloc::success;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t unused(shared), tx_t tx, void* target) noexcept {
    TX
    #ifdef _DEBUG_
        cout << "TM_FREE starts\n";
    #endif
    transaction->freeBuffer.push_back((index(target)));
    return true;
}
