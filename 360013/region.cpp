#include <region.hpp>

/*----- SEGMENT -----*/

Segment::Segment(size_t _align, size_t _size) : size(_size) {
    // Allocate the array of locks (1 lock per word)
    int numLocks = _size / _align;
    locks = new (nothrow) atomic_int[numLocks];
    if (unlikely(!locks)) {
        throw new runtime_error("Failed to allocate the locks\n");
    }
    for (int i = 0; i < numLocks; i++) {
        locks[i].store(0);
    }
    // Allocate the memory given the alignment
    if (unlikely(posix_memalign(&data, _align, _size))) {
        throw new runtime_error("Failed to allocate the given segment\n");
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
        cout << "Creating the region\n";
    #endif

    // Initialize the lock and mutex
    if (unlikely(pthread_mutex_init(&freeLock, nullptr)     || 
                 pthread_mutex_init(&memoryLock, nullptr)   ||
                 pthread_rwlock_init(&cleanLock, nullptr))) {
        throw new runtime_error("Failed to allocate the mutex\n");
    }

    // Preallocate an array of segments
    memory = (Segment**) malloc(MAXSEGMENTS * sizeof(Segment*));
    if (unlikely(!memory)) {
        throw new runtime_error("Failed to allocate the region\n");
    }

    // Allocate the first segment
    try {
        memory[0] = new Segment(_align, _size);
    } catch (const runtime_error& e) {
        free(memory);
        throw new runtime_error("Failed to allocate the first segment\n");
    }

    #ifdef _DEBUG_
        cout << "Region correctly created, size: " << _size << "align: " << align << "\n";
    #endif
}

int Region::getCount() {
    int tmp;
    pthread_mutex_lock(&memoryLock);
    if (!missingIdx.empty()) {
        tmp = missingIdx.front();
        missingIdx.pop();
    } else {
        tmp = countSegments++;
    }
    pthread_mutex_unlock(&memoryLock);
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