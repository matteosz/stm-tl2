/**
 * @file   tm.cpp
 * @author Matteo Suez
 * @section DESCRIPTION
 * Implementation of my transaction manager based on TL2.
**/

// Requested features
#ifdef __STDC_NO_ATOMICS__
    #error Current C11 compiler does not support atomic operations
#endif

// Internal headers
#include <transaction.hpp>

static thread_local Transaction tr(false);

shared_t tm_create(size_t size, size_t align) noexcept {
    Region *region = new Region(size, align);
    if (unlikely(!region)) {
        return invalid_shared;
    }
    return region;
}

void tm_destroy(shared_t shared) noexcept {
    delete (Region*) shared;
}

void* tm_start(shared_t shared) noexcept {
    return ((Region*) shared)->start;
}

size_t tm_size(shared_t shared) noexcept {
    return ((Region*) shared)->size;
}

size_t tm_align(shared_t shared) noexcept {
    return ((Region*) shared)->align;
}

tx_t tm_begin(shared_t shared, bool is_ro) noexcept {
    tr.begin((Region*) shared, is_ro);
    return (tx_t) &tr;
}

bool tm_end(shared_t shared, tx_t unused(tx)) noexcept {
    Region *region = (Region*) shared;
    uint32_t count;

    if (tr.rOnly || tr.isEmpty() || !tr.acquire(region, &count)) {
        tr.clear();
        return (tr.rOnly || tr.isEmpty())? true : false;
    }

    tr.setWVersion(region);
    
    if ((tr.rVersion != tr.wVersion - 1) && !tr.validate(region)) {
        tr.release(region, count);
        tr.clear();
        return false;
    }

    return tr.commit(region);
}

bool tm_read(shared_t shared, tx_t unused(tx), void const* source, size_t size, void* target) noexcept {
    Region *region = (Region*) shared;
    tx_t src = (tx_t) source, dst = (tx_t) target;

    for (size_t wordNum = 0; wordNum < size / region->align; wordNum++) {
        tx_t srcWord = src + wordNum * region->align;
        Word &word = region->getWord(srcWord);
        void *dstWord = (void*) (dst + wordNum * region->align);

        if (tr.search(srcWord, dstWord, region->align)) {
            continue;
        }

        Version before = word.sampleLock();
        memcpy(dstWord, &word.value, region->align);

        // Sample the lock again to check if a concurrent transaction has occurred
        Version after = word.sampleLock();

        // If the word has been locked after, or the 2 version numbers are different (or greater than readVersion)
        if (after.lock || (before.versionNumber != after.versionNumber) || (before.versionNumber > tr.rVersion)) {
            tr.clear();
            return false;
        }

        tr.insertReadSet(srcWord);
    }
    return true;
}

bool tm_write(shared_t shared, tx_t unused(tx), void const* source, size_t size, void* target) noexcept {
    Region *region = (Region*) shared;
    tx_t src = (tx_t) source, dst = (tx_t) target;

    for (size_t wordNum = 0; wordNum < size / region->align; wordNum++) {
        void *srcWord = (void*) (src + wordNum * region->align), *cp = malloc(region->align);
        tx_t dstWord = dst + wordNum * region->align;

        // Copy the content of the source to a temporary memory region
        memcpy(cp, srcWord, region->align);

        // Insert that address into the writeSet
        tr.insertWriteSet(dstWord, cp);
    }
    return true;
}

Alloc tm_alloc(shared_t shared, tx_t unused(tx), size_t unused(size), void** target) noexcept {
    *target = ((Region*) shared)->getAddress();
    return Alloc::success;
}

bool tm_free(shared_t unused(shared), tx_t unused(tx), void* unused(target)) noexcept {
    return true;
}
