#include <transaction.hpp>

Transaction::Transaction() : readOnly(false) {}

bool Transaction::validate() {
    for (const auto &address : readSet) {
        int vers = getVersion(address);
        // Check if the address has write-lock taken or version > read version
        // If it's not locked (MSB=0) then it's safe to simply cast
        if ((vers < 0) || (vers > readVersion)) {
            return false;
        }
    }
    return true;
} 

bool Transaction::commit(Region *region, atomic_int* globalClock) {
    // If it's read only or the write set is empty can commit
    if (readOnly || writeSet.empty()) {
        #ifdef _DEBUG_
            cout << "Committing readonly transaction or empty write set\n";
        #endif
        COMMIT
    }

    // Otherwise try to acquire all the locks, if fails abort
    int count = 0;
    if (!acquireLocks(&count)) {
        #ifdef _DEBUG_
            cout << "Failed to acquire the locks during commit\n";
        #endif
        _ABORT
    }

    // From here we acquired the locks, it's important to free them later on (even if abort)

    // Increment gloabl version clock
    int writeVersion = atomic_fetch_add(globalClock, 1) + 1;

    #ifdef _DEBUG_
        cout << "Locks acquired, new wv: " << writeVersion << "\n"; 
    #endif

    // Special case check rv == wv - 1: no need to validate the read set
    if ((readVersion != writeVersion - 1) && !validate()) {
        #ifdef _DEBUG_
            cout << "Failed to validate the read set, releasing the locks and aborting...\n";
        #endif
        releaseLocks(count);
        _ABORT
    }

    #ifdef _DEBUG_
        cout << "Validation passed\n"; 
    #endif

    // Now we can commit and release the locks
    for (const auto &pair : writeSet) {
        // Copy in our shared memory the content of the write set
        memcpy(pair.second.raw, pair.second.content, region->align);
        
        // Set the new version and free the lock
        update(pair.second.lock, writeVersion);
    }

    // Now it's time to free the segments allocated
    if (!freeBuffer.empty()) {
        // Lock to insert in the region the free buffer
        pthread_mutex_lock(&region->freeLock);

        region->freeBuffer.insert(region->freeBuffer.end(), 
                                  freeBuffer.begin(), freeBuffer.end());

        // Batch the free for improved performance
        if (region->freeBuffer.size() > BATCH) {
            #ifdef _DEBUG_
                cout << "Batched free starts...\n"; 
            #endif

            auto it = region->freeBuffer.begin();
            while (it != region->freeBuffer.end()) {
                Segment *segment = region->memory[*it];
                region->memory[*it] = nullptr;
                delete segment;
                region->missingIdx.push(*it);
                it = region->freeBuffer.erase(it);
            }
        }

        pthread_mutex_unlock(&region->freeLock);
    }

    COMMIT
}

void Transaction::clean() {
    for (const auto &item : writeSet) {
        if (item.second.content) {
            free(item.second.content);
        }
    }
    readOnly = false;
    readVersion = 0;
    readSet.clear();
    writeSet.clear();
    #ifdef _DEBUG_
        cout << "Transaction correctly cleaned\n"; 
    #endif
}


/* ----- PRIVATE -----*/

bool Transaction::acquireLocks(int *count) {
    // Attempt to acquire the locks of the write set
    // Save the number of locks acquired to release later on
    for (const auto &pair : writeSet) {
        if (!acquire(pair.second.lock, readVersion)) {
            releaseLocks(*count);
            return false;
        }
        ++*count;
    }
    return true;
}

void Transaction::releaseLocks(int count) {
    if (count < 1) {
        return;
    }
    for (const auto &pair : writeSet) {
        removeLock(pair.second.lock);
        if (--count < 1) {
            break;
        }
    }
}