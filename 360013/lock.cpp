#include <lock.hpp>

Version::Version(uint64_t _versionNumber, uint64_t _versionLock, uint64_t _lock) : 
                versionNumber(_versionNumber), versionLock(_versionLock), lock(_lock == 1) {}
                
Lock::Lock() : version(0) {}
Lock::Lock(const Lock &_lock) : version(_lock.version.load()) {}

bool Lock::acquire() {
    uint64_t _version = version.load(),
             // 011...111 & _version -> copy the version except first bit (lock)
             unlocked = bitMask & _version;
    
    /** The lock is represented by the first bit of the version
     *  1 -> taken, 0 -> not taken already
     */
    if ((_version >> longShift) == 1) {
        #ifdef DEBUG
            cout << "Impossible to acquire, locked already: version = " << bitset<64>(_version) << "\n"; 
        #endif
        return false;
    }

    // Perform c&s in case of concurrent access to the lock after the check
    return compareAndSwap(true, unlocked, _version);
}

bool Lock::release() {
    uint64_t _version = version.load(),
             unlocked = bitMask & _version;

    // If the lock has already been released (or never taken)
    if ((_version >> longShift) != 1) {
        #ifdef DEBUG
            cout << "Impossible to release, not locked: version = " << bitset<64>(_version) << "\n"; 
        #endif
        return false;
    }
    
    // Perform c&s in case of concurrent access to the lock after the check
    return compareAndSwap(false, unlocked, _version);
}

bool Lock::setVersion(uint64_t newVersion) {
    uint64_t oldVersion = version.load();

    // If the lock has already been released (or never taken)
    if ((oldVersion >> longShift) != 1) {
        #ifdef DEBUG
            cout << "Impossible to set new version, locked already: old_version = " << bitset<64>(oldVersion) << "\n"; 
        #endif
        return false;
    }

    return compareAndSwap(false, newVersion, oldVersion);
}

Version Lock::sampleLock() {
    uint64_t _version = version.load();
    return Version(bitMask & _version, 
                    _version, 
                    _version >> longShift);
}

bool Lock::compareAndSwap(bool lock, uint64_t newValue, uint64_t oldValue) {
    return version.compare_exchange_strong(oldValue, getVersion(lock, newValue));
}

uint64_t Lock::getVersion(bool lock, uint64_t newValue) {
    if ((newValue >> longShift) == 1) {
        #ifdef DEBUG
            cout << "Too big version = " << bitset<64>(newValue) << "\n"; 
        #endif
        throw -1;
    }

    // Inglobe the lock bit in the version number
    if (lock) {
        #ifdef DEBUG
            cout << "Lock was true, concat version = " << bitset<64>(firstBitMask | newValue) << "\n"; 
        #endif
        return firstBitMask | newValue;
    }
    return newValue;
}
