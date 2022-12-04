#pragma once

// Internal header
#include <lock.hpp>

// Shorthand macro
#define REG Region *region = (Region*) shared;

class Segment {
    public:
        size_t size;
        void *data;
        atomic_int *locks;

        Segment(size_t,size_t);
        ~Segment();
};

class Region {
    public:
        atomic_int globalClock;
        size_t align;
        int countSegments;
        Segment **memory;

        vector<int> freeBuffer;
        queue<int> missingIdx;
        pthread_mutex_t memoryLock, freeLock;
        pthread_rwlock_t cleanLock;

        Region(size_t,size_t);
        int getCount();
        ~Region();
};

