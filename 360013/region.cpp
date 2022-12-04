#include <region.hpp>

/*----- SEGMENT -----*/

Segment::Segment(size_t _align, size_t _size) : size(_size) {
    // Allocate the array of locks (1 lock per word)
    int numLocks = _size / _align;
    try {
        locks = new atomic_int[numLocks];
    } catch (const bad_alloc& e) {
        #ifdef _DEBUG_
            cout << "Failed to allocate the array of locks in the given segment\n";
        #endif
        throw e;
    }
    for (int i = 0; i < numLocks; i++) {
        locks[i].store(0);
    }
    // Allocate the memory given the alignment
    if (unlikely(posix_memalign(&data, _align, _size))) {
        #ifdef _DEBUG_
            cout << "Failed to allocate with posix memalign in the given segment\n";
        #endif
        throw new bad_alloc();
    }

    // posix_memalign doesn't initialize to 0 the memory allocated, necessary to do it manually
    memset(data, 0, _size); 
}

Segment::~Segment() {
    free(data);
    delete[] locks;
    #ifdef _DEBUG_
        cout << "Segment correctly destroyed\n";
    #endif
}

/*----- REGION -----*/

Region::Region(size_t _align, size_t _size) : 
                                            globalClock(0), 
                                            align(_align), 
                                            countSegments(1) {
    #ifdef _DEBUG_
        cout << "TM_CREATE-> s:" << _size << " a:" << _align << "\n";
    #endif

    // Initialize the lock and mutex
    if (unlikely(pthread_mutex_init(&freeLock, nullptr)     || 
                 pthread_mutex_init(&memoryLock, nullptr)   ||
                 pthread_rwlock_init(&cleanLock, nullptr))) {
        #ifdef _DEBUG_
            cout << "Failed to allocate the mutex and locks\n";
        #endif
        throw new bad_alloc();
    }

    // Preallocate an array of segments
    memory = (Segment**) malloc(MAXSEGMENTS * sizeof(Segment*));
    if (unlikely(!memory)) {
        #ifdef _DEBUG_
            cout << "Failed to allocate the array of segments\n";
        #endif
        throw new bad_alloc();
    }

    // Allocate the first segment
    try {
        memory[0] = new Segment(_align, _size);
    } catch (const bad_alloc& e) {
        #ifdef _DEBUG_
            cout << "Failed to allocate the first segment\n";
        #endif
        free(memory);
        throw e;
    }

    #ifdef _DEBUG_
        cout << "TM_CREATED OK\n";
    #endif
}

int Region::getCount() {
    int tmp;
    #ifdef _DEBUG_
        cout << "Taking memory lock\n";
    #endif
    pthread_mutex_lock(&memoryLock);
    if (!missingIdx.empty()) {
        tmp = missingIdx.front();
        missingIdx.pop();
    } else {
        tmp = countSegments++;
    }
    pthread_mutex_unlock(&memoryLock);
    #ifdef _DEBUG_
        cout << "Unlocked memory lock, count=" << tmp << "\n";
    #endif
    return tmp;
}

Region::~Region() {
    for (int i = 0; i < MAXSEGMENTS; i++) {
        if (memory[i]) {
            free(memory[i]);
        }
    }
    free(memory);
    pthread_rwlock_destroy(&cleanLock);
    pthread_mutex_destroy(&memoryLock);
    pthread_mutex_destroy(&freeLock);
    #ifdef _DEBUG_
        cout << "Region correctly destroyed\n";
    #endif
}