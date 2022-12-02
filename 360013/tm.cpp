/**
 * @file   tm.cpp
 * @author Matteo Suez <matteo.suez@epfl.ch>
 * 
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * any later version. Please see https://gnu.org/licenses/gpl.html
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * @section DESCRIPTION
 *
 * TL2 transaction manager implementation.
**/

// Requested features
#ifdef __STDC_NO_ATOMICS__
    #error Current C11 compiler does not support atomic operations
#endif

// Internal headers
#include "transaction.hpp"

static atomic_uint64_t globalClock(0);
static thread_local Transaction tr(false);

/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/
shared_t tm_create(size_t size, size_t align) noexcept {
    Region *region = new Region(size, align);
    if (unlikely(!region)) {
        return invalid_shared;
    }
    #ifdef _DEBUG_
        cout << "Region created (size,align): " << size << " " << align << "\n";
    #endif
    return region;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t shared) noexcept {
    #ifdef _DEBUG_
        cout << "Region destroyed\n";
    #endif
    delete (Region*) shared;
}

/** [thread-safe] Return the start address of the first allocated segment in the shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
**/
void* tm_start(shared_t shared) noexcept {
    #ifdef _DEBUG_
        cout << "Region start address: " << ((Region*) shared)->start << "\n";
    #endif
    return ((Region*) shared)->start;
}

/** [thread-safe] Return the size (in bytes) of the first allocated segment of the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
**/
size_t tm_size(shared_t shared) noexcept {
    #ifdef _DEBUG_
        cout << "Region size: " << ((Region*) shared)->size << "\n";
    #endif
    return ((Region*) shared)->size;
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
**/
size_t tm_align(shared_t shared) noexcept {
    #ifdef _DEBUG_
        cout << "Region alignment: " << ((Region*) shared)->align << "\n";
    #endif
    return ((Region*) shared)->align;
}

/** [thread-safe] Begin a new transaction on the given shared memory region.
 * @param shared Shared memory region to start a transaction on
 * @param is_ro  Whether the transaction is read-only
 * @return Opaque transaction ID, 'invalid_tx' on failure
**/
tx_t tm_begin(shared_t unused(shared), bool is_ro) noexcept {
    tr.begin(&globalClock, is_ro);
    #ifdef _DEBUG_
        cout << "Transaction started: rv = " << tr.rVersion << " ro = " << is_ro << "\n";
    #endif
    return (tx_t) &tr;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t shared, tx_t unused(tx)) noexcept {
    Region *region = (Region*) shared;
    uint32_t count = 0;

    if (tr.rOnly || tr.isEmpty()) {
        tr.clear();
        #ifdef _DEBUG_
            cout << "Transaction ended correctly\n";
        #endif
        return true;
    }

    if (!tr.acquire(region, &count)) {
        tr.clear();
        #ifdef _DEBUG_
            cout << "Transaction failed to acquire, impossible to commit\n";
        #endif
        return false;
    }

    tr.setWVersion(&globalClock);
    #ifdef _DEBUG_
        cout << "Set new writeVersion: " << tr.wVersion << ", readVersion: " 
        << tr.rVersion << "\n";
    #endif
    
    if ((tr.rVersion != tr.wVersion - 1) && !tr.validate(region)) {
        tr.release(region, count);
        tr.clear();
        #ifdef _DEBUG_
            cout << "Released transaction - failed to validate\n";
        #endif
        return false;
    }

    return tr.commit(region);
}

/** [thread-safe] Read operation in the given transaction, source in the shared region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
**/
bool tm_read(shared_t shared, tx_t unused(tx), void const* source, size_t size, void* target) noexcept {
    Region *region = (Region*) shared;
    tx_t src = (tx_t) source, dst = (tx_t) target;

    for (size_t wordNum = 0; wordNum < size / region->align; wordNum++) {
        tx_t srcWord = src + wordNum * region->align;
        void *dstWord = (void*) (dst + wordNum * region->align);

        if (tr.search(srcWord, dstWord, region->align)) {
            continue;
        }

        #ifdef _DEBUG_
            cout << "Address not found on write set, trying to sample lock...\n";
        #endif

        Word &word = region->getWord(srcWord);
        Version before = word.sampleLock();

        if (before.lock) {
            #ifdef _DEBUG_
                cout << "Address already locked - Stopped\n";
            #endif
            tr.clear();
            return false;
        }

        // Copy the content of the word in the private memory
        memcpy(dstWord, &word.value, region->align);

        #ifdef _DEBUG_
            cout << "Content copied in memory, trying to resample lock...\n";
        #endif

        // Sample the lock again to check whether a concurrent transaction has occurred
        Version after = word.sampleLock();

        // If the word has been locked after, or the 2 version numbers are different (or greater than readVersion)
        if (after.lock || (before.versionNumber != after.versionNumber) || (!tr.rOnly && after.versionNumber > tr.rVersion)) {
            tr.clear();
            return false;
        }

        if (tr.rOnly && after.versionNumber > tr.rVersion) {
            #ifdef _DEBUG_
                cout << "Lock free but version number: " << after.versionNumber
                << " > readVersion (" << tr.rVersion << "). Let's revalidate...\n";
            #endif
            uint64_t sample = globalClock.load();
            if (tr.validate(region)) {
                #ifdef _DEBUG_
                    cout << "Rivalidation executed correctly: new readVersion: " << 
                    sample << "\n";
                #endif
                tr.setRVersion(sample);
                continue;
            } else {
                #ifdef _DEBUG_
                    cout << "Rivalidation failed\n";
                #endif
                tr.clear();
                return false;
            }
        }
    
        tr.insertReadSet(srcWord);
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
bool tm_write(shared_t shared, tx_t unused(tx), void const* source, size_t size, void* target) noexcept {
    Region *region = (Region*) shared;
    tx_t src = (tx_t) source, dst = (tx_t) target;

    for (size_t wordNum = 0; wordNum < size / region->align; wordNum++) {
        tx_t dstWord = dst + wordNum * region->align;
        void *srcWord = (void*) (src + wordNum * region->align), *cp = malloc(region->align);

        // Copy the content of the source to a temporary memory region
        memcpy(cp, srcWord, region->align);

        // Insert that address into the writeSet
        tr.insertWriteSet(dstWord, cp);
        #ifdef _DEBUG_
            cout << "Write: Inserted in writeSet the address " << cp << 
            "mapped by the virtual shared address " << dstWord << "\n";
        #endif
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
Alloc tm_alloc(shared_t shared, tx_t unused(tx), size_t unused(size), void** target) noexcept {
    *target = ((Region*) shared)->getAddress();
    #ifdef _DEBUG_
        cout << "Transaction allocated at address:" << *target << "\n";
    #endif
    return Alloc::success;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t unused(shared), tx_t unused(tx), void* unused(target)) noexcept {
    return true;
}
