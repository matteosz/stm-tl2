/**
 * @file   tm.cpp
 * @author Matteo Suez
 * @section DESCRIPTION
 * Implementation of my transaction manager based on TL2.
**/

// Requested features
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L
#ifdef __STDC_NO_ATOMICS__
    #error Current C11 compiler does not support atomic operations
#endif

// Internal headers
#include <transaction.hpp>

static atomic_uint64_t globalClock(0);
static thread_local Transaction tr;

shared_t tm_create(size_t size, size_t align) noexcept {
    Region *region = new Region(size, align);
    /*if (unlikely(!region)) {
        #ifdef _DEBUG_
            cout << "Region failed to create\n";
        #endif
        return invalid_shared;
    }*/
    #ifdef _DEBUG_
        cout << "Region created (size,align): " << size << " " << align << "\n";
    #endif
    return region;
}

void tm_destroy(shared_t shared) noexcept {
    #ifdef _DEBUG_
        cout << "Region destroyed\n";
    #endif
    delete (Region*) shared;
}

void* tm_start(shared_t unused(shared)) noexcept {
    #ifdef _DEBUG_
        cout << "Region start address: " << ((Region*) shared)->start << "\n";
    #endif
    return reinterpret_cast<void*>(firstAddress);
}

size_t tm_size(shared_t shared) noexcept {
    #ifdef _DEBUG_
        cout << "Region size: " << ((Region*) shared)->size << "\n";
    #endif
    return ((Region*) shared)->size;
}

size_t tm_align(shared_t shared) noexcept {
    #ifdef _DEBUG_
        cout << "Region alignment: " << ((Region*) shared)->align << "\n";
    #endif
    return ((Region*) shared)->align;
}

tx_t tm_begin(shared_t unused(shared), bool is_ro) noexcept {
    tr.rVersion = globalClock.load();
    tr.rOnly = is_ro;
    /*#ifdef _DEBUG_
        cout << "Transaction started: rv = " << tr.rVersion << " ro = " << is_ro << "\n";
    #endif*/
    return (tx_t) &tr;
}

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
        Version before = word.lock.sampleLock();

        /*if (before.lock) {
            #ifdef _DEBUG_
                cout << "Address already locked - Stopped\n";
            #endif
            tr.clear();
            return false;
        }*/

        memcpy(dstWord, &word.value, region->align);

        #ifdef _DEBUG_
            cout << "Content copied in memory, trying to resample lock...\n";
        #endif

        // Sample the lock again to check if a concurrent transaction has occurred
        Version after = word.lock.sampleLock();

        // If the word has been locked after, or the 2 version numbers are different (or greater than readVersion)
        if (after.lock || (before.versionNumber != after.versionNumber) || (after.versionNumber > tr.rVersion)) {
            tr.clear();
            return false;
        }

        /*if (tr.rOnly && after.versionNumber > tr.rVersion) {
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
        }*/

        tr.insertReadSet(srcWord);
    }
    return true;
}

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

Alloc tm_alloc(shared_t shared, tx_t unused(tx), size_t unused(size), void** target) noexcept {
    *target = ((Region*) shared)->getAddress();
    #ifdef _DEBUG_
        cout << "Transaction allocated at address:" << *target << "\n";
    #endif
    return Alloc::success;
}

bool tm_free(shared_t unused(shared), tx_t unused(tx), void* unused(target)) noexcept {
    return true;
}
