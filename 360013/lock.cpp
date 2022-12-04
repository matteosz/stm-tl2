#include <lock.hpp>

bool acquire(atomic_int *_lock) {
    int lock = _lock->load();
    
    /** The lock is represented by the first bit of the version
     *  1 -> taken, 0 -> not taken already
     */
    if (isLocked(lock)) {
        #ifdef _DEBUG_
            cout << "Impossible to acquire, locked already: version = " << bitset<32>(lock) << "\n"; 
        #endif
        return false;
    }

    // Perform c&s in case of concurrent access to the lock after the check
    return _lock->compare_exchange_strong(lock, addLock(lock));
}

void release(atomic_int *_lock) {
    int lock = _lock->load();

    // If the lock has already been released (or never taken)
    if (!isLocked(lock)) {
        #ifdef _DEBUG_
            cout << "Impossible to release, not locked: version = " << bitset<32>(lock) << "\n"; 
        #endif
        return;
    }
    
    // Perform c&s in case of concurrent access to the lock after the check
    _lock->compare_exchange_strong(lock, unlock(lock));
}

bool setVersion(atomic_int *_lock, int newVersion) {
    int lock = _lock->load(); 

    if (!isLocked(lock)) {
        #ifdef _DEBUG_
            cout << "Impossible to set, not locked: version = " << bitset<32>(lock) << "\n"; 
        #endif
        return false;
    }
    // The new version will have the MSB cleared, not needed to convert
    // Perform c&s in case of concurrent access to the lock after the check
    return _lock->compare_exchange_strong(lock, newVersion);
}