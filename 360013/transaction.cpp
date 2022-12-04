#include <transaction.hpp>

Transaction::Transaction(bool _readOnly, Region *region) : 
                                                        readOnly(_readOnly), 
                                                        readVersion(region->globalClock.load()) {
    // Take the SHARED read lock 
    pthread_rwlock_rdlock(&region->cleanLock);
}

bool Transaction::validate() {
    for (const auto &address : readSet) {
        int lock = address->load();
        // Check if the address has write-lock taken or version > read version
        // If it's not locked (MSB=0) then it's safe to simply cast
        if (isLocked(lock) || (lock > readVersion)) {
            return false;
        }
    }
    return true;
}

void Transaction::clean(Region *region, bool unlock) {
    if (unlock) {
        pthread_rwlock_unlock(&region->cleanLock);
    }
    for (const auto &item : writeSet) {
        if (item.second) {
            free(item.second);
        }
    }
    //delete this;
}

bool Transaction::commit(Region *region) {
    // If it's read only or the write set is empty can commit
    if (readOnly || writeSet.empty()) {
        pthread_rwlock_unlock(&region->cleanLock);
        COMMIT
    }

    // Otherwise try to acquire all the locks, if fails abort
    int count = 0;
    if (!acquireLocks(region, &count)) {
        #ifdef _DEBUG_
            cout << "Failed to acquire the locks during commit\n";
        #endif
        _ABORT
    }

    // From here we acquired the locks, it's important to free them later on (even if abort)

    // Increment gloabl version clock
    int writeVersion = region->globalClock.fetch_add(1) + 1;

    #ifdef _DEBUG_
        cout << "Lock acquired, new wv: " << writeVersion << "\n"; 
    #endif

    // Special case check rv == wv - 1: no need to validate the read set
    if ((readVersion != writeVersion - 1) && !validate()) {
        #ifdef _DEBUG_
            cout << "Failed to validate the read set, releasing the locks and aborting...\n";
        #endif
        releaseLocks(region, count);
        _ABORT
    }

    // Now we can commit and release the locks
    for (const auto &[key, value] : writeSet) {
        Segment *segment = region->memory[index(key)];
        size_t offset = offset(key);
        void *dest = (void*) ((size_t) segment->data + offset);

        // Copy in our shared memory the content of the write set
        memcpy(dest, value, region->align);
        
        // Set the new version and free the lock
        if (!setVersion(&segment->locks[offset / region->align], writeVersion)) {
            _ABORT
        }
    }
    
    pthread_rwlock_unlock(&region->cleanLock);

    // Now it's time to free the segments allocated
    if (!freeBuffer.empty()) {
        pthread_mutex_lock(&region->freeLock);

        region->freeBuffer.insert(region->freeBuffer.end(), 
                                  freeBuffer.begin(), freeBuffer.end());

        // Batch the free for improved performance
        if (region->freeBuffer.size() > BATCH) {
            pthread_rwlock_wrlock(&region->cleanLock);

            auto it = region->freeBuffer.begin();
            while (it != region->freeBuffer.end()) {
                Segment *segment = region->memory[*it];
                region->memory[*it] = nullptr;
                delete segment;
                region->missingIdx.push(*it);
                it = region->freeBuffer.erase(it);
            }

            pthread_rwlock_unlock(&region->cleanLock);
        }

        pthread_mutex_unlock(&region->freeLock);
    }

    COMMIT
}

/* ----- PRIVATE -----*/

bool Transaction::acquireLocks(Region* region, int *count) {
    // Attempt to acquire the locks of the write set
    // Save the number of locks acquired to release later on
    for (const auto &pair : writeSet) {
        if (!acquire(&region->memory[index(pair.first)]
                            ->locks[offset(pair.first) / region->align])) {
            releaseLocks(region, *count);
            return false;
        }
        ++*count;
    }
    return true;
}

void Transaction::releaseLocks(Region* region, int count) {
    if (!count) {
        return;
    }
    for (const auto &pair : writeSet) {
        release(&region->memory[index(pair.first)]
                       ->locks[offset(pair.first) / region->align]);
        if (!--count) {
            break;
        }
    }
}