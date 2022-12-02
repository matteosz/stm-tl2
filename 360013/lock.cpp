#include <lock.hpp>

static uint64_t getVersion(bool lock, uint64_t newValue) {
    /*if (newValue >> longShift) {
        #ifdef _DEBUG_
            cout << "Too big version = " << bitset<64>(newValue) << "\n"; 
        #endif
        throw new exception();
    }*/
    // Inglobe the lock bit in the version number
    if (lock) {
        #ifdef _DEBUG_
            cout << "Lock was true, concat version = " << bitset<64>(firstBitMask | newValue) << "\n"; 
        #endif
        return firstBitMask | newValue;
    }
    return newValue;
}

Version::Version(uint64_t _versionNumber, uint64_t _versionLock, bool _lock) : 
                versionNumber(_versionNumber), versionLock(_versionLock), lock(_lock) {}

bool Version::valid(uint64_t rVersion) {
    return (!lock && (versionNumber <= rVersion));
}

Lock::Lock() : version(0) {}
Lock::Lock(const Lock &_lock) : version(_lock.version.load()) {}

bool Lock::acquire() {
    uint64_t _version = version.load(),
             // 011...111 & _version -> copy the version except first bit (lock)
             unlocked = bitMask & _version;
    
    /** The lock is represented by the first bit of the version
     *  1 -> taken, 0 -> not taken already
     */
    if (_version >> longShift) {
        #ifdef _DEBUG_
            cout << "Impossible to acquire, locked already: version = " << bitset<64>(_version) << "\n"; 
        #endif
        return false;
    }

    // Perform c&s in case of concurrent access to the lock after the check
    return version.compare_exchange_weak(_version, getVersion(true, unlocked));
}

bool Lock::release() {
    uint64_t _version = version.load(),
             unlocked = bitMask & _version;

    // If the lock has already been released (or never taken)
    if (!(_version >> longShift)) {
        #ifdef _DEBUG_
            cout << "Impossible to release, not locked: version = " << bitset<64>(_version) << "\n"; 
        #endif
        return false;
    }
    
    // Perform c&s in case of concurrent access to the lock after the check
    return version.compare_exchange_weak(_version, getVersion(false, unlocked));
}

bool Lock::setVersion(uint64_t newVersion) {
    uint64_t oldVersion = version.load();

    // If the lock has already been released (or never taken)
    if (!(oldVersion >> longShift)) {
        #ifdef _DEBUG_
            cout << "Impossible to set new version, locked already: old_version = " << bitset<64>(oldVersion) << "\n"; 
        #endif
        return false;
    }

    return version.compare_exchange_weak(oldVersion, getVersion(false, newVersion));
}

Version Lock::sampleLock() {
    uint64_t _version = version.load();
    return Version(bitMask & _version, 
                    _version, 
                    _version >> longShift);
}