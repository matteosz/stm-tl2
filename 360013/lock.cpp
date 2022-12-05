#include <lock.hpp>

bool acquire(atomic_int *_lock, int readVersion) {
    int lock = atomic_load(_lock);
    
    /** The lock is represented by the first bit of the version
     *  1 -> taken, 0 -> not taken already
     */
    if ((lock & 0x1) || (lock >> 1) > readVersion) {
        #ifdef _DEBUG_
            cout << "Impossible to acquire, locked already: version = " << bitset<32>(lock) << "\n"; 
        #endif
        return false;
    }

    // Perform c&s in case of concurrent access to the lock after the check
    return atomic_compare_exchange_strong(_lock, &lock, lock | 0x1);
}

int getVersion(atomic_int *_lock) {
  int lock = atomic_load(_lock);
  if (lock & 0x1) {
    return -1;
  }
  return lock >> 1;
}

void removeLock(atomic_int *lock) { 
    atomic_fetch_sub(lock, 1); 
}
/*
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
}*/

void update(atomic_int *_lock, int newVersion) {
    /*int lock = _lock->load(); 

    if (!isLocked(lock)) {
        #ifdef _DEBUG_
            cout << "Impossible to set, not locked: version = " << bitset<32>(lock) << "\n"; 
        #endif
        return false;
    }
    // The new version will have the MSB cleared, not needed to convert
    // Perform c&s in case of concurrent access to the lock after the check
    return _lock->compare_exchange_strong(lock, newVersion);*/
    atomic_store(_lock, newVersion << 1);
}