/**
 * @file   tm.c
 * @author Matteo Suez
 * @section DESCRIPTION
 *
 * Implementation of my transaction manager based on TL2.
**/

#pragma once

// Requested features
#define _GNU_SOURCE
#ifdef __STDC_NO_ATOMICS__
    #error Current C11 compiler does not support atomic operations
#endif

using namespace std;

// External headers
#include <atomic>
#include <vector>

// Internal headers
#include "tm.hpp"
#include "macros.hpp"
#include "transaction.hpp"
#include "word.hpp"

static thread_local Transaction tr;

class Region {
    public:
        void *start;
        size_t size, align;
        atomic_uint64_t globalClock, nextSegment;
        vector<vector<Word>> matrix;

        Region(size_t size, size_t align) : 
            start((void*) (reference << bitShift)), 
            size(size), 
            align(align), 
            nextSegment(2), 
            matrix(m, vector<Word>(n)) {}

        uint64_t sampleClock() {
            return this->globalClock.load();
        }
        Word* getWord(tx_t address) {
            return &(this->matrix[getRow(address)][getCol(address)]);
        }
        uint64_t incrementSegments() {
            return this->nextSegment.fetch_add(1);
        }
        uint64_t incrementClock() {
            return this->globalClock.fetch_add(1);
        }

    private:
        uint16_t getRow(tx_t address) {
            return address >> bitShift;
        }
        uint16_t getCol(tx_t address) {
            return ((address << bitShift) >> bitShift) / this->align;
        }
};

shared_t tm_create(size_t size, size_t align) {
    Region *region = new Region(size, align);
    if (unlikely(!region)) {
        return invalid_shared;
    }
    return region;
}

void tm_destroy(shared_t shared) {
    delete (Region*) shared;
}

void* tm_start(shared_t shared) {
    return ((Region*) shared)->start;
}

size_t tm_size(shared_t shared) {
    return ((Region*) shared)->size;
}

size_t tm_align(shared_t shared) {
    return ((Region*) shared)->align;
}

tx_t tm_begin(shared_t shared, bool is_ro) {
    tr.setClock((Region*) shared);
    tr.setRo(is_ro);
    return (tx_t) &tr;
}

bool tm_end(shared_t shared, tx_t unused(tx)) {
    Region *region = (Region*) shared;
    int count = 0;
    if (tr.rOnly || tr.isEmpty() || tr.acquire(region, &count)) {
        tr.clear();
        return true;
    }
    tr.setWVersion(region->incrementClock());
    if ((tr.rVersion != tr.wVersion - 1) && !tr.validate(region)) {
        tr.release(region, count);
        tr.clear();
        return false;
    }

    return tr.commit(region);
}

bool tm_read(shared_t shared, tx_t unused(tx), void const* source, size_t size, void* target) {
    Region *region = (Region*) shared;
    tx_t src = (tx_t) source;

    for (size_t wordNum = 0; wordNum < size / region->align; wordNum++) {
        tx_t sourceWord = src + wordNum * region->align;
        void *targetWord = target + wordNum * region->align;

        if (!tr.rOnly) {
            auto search = tr.search(sourceWord);
            if (search != tr.wEnd()) {
                memcpy(targetWord, search->second, region->align);
                continue;
            }
        }

        Word *word = region->getWord(sourceWord);
        Lock::Version before = word->sampleLock();
        memcpy(targetWord, &word->value, region->align);
        Lock::Version after = word->sampleLock();

        if (after.lock || before.versionNumber != after.versionNumber || before.versionNumber > tr.rVersion) {
            tr.clear();
            return false;
        }

        if (!tr.rOnly) {
            tr.insertReadSet(sourceWord);
        }
    }

    return true;
}

bool tm_write(shared_t shared, tx_t unused(tx), void const* source, size_t size, void* target) {
    Region *region = (Region*) shared;
    tx_t dst = (tx_t) target;

    for (size_t wordNum = 0; wordNum < size / region->align; wordNum++) {
        void *sourceWord = (void*) ((tx_t) source + wordNum * region->align), *cp = malloc(region->align);
        tx_t targetWord = dst + wordNum * region->align;
        memcpy(cp, sourceWord, region->align);
        tr.insertWriteSet(targetWord, cp);
    }

    return true;
}

Alloc tm_alloc(shared_t shared, tx_t unused(tx), size_t unused(size), void** target) {
    *target = (void*) (((Region*) shared)->incrementSegments() << bitShift);
    return Alloc::success;
}

bool tm_free(shared_t unused(shared), tx_t unused(tx), void* unused(target)) {
    return true;
}
