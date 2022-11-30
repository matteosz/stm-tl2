#include <lock.hpp>

#include <cstdint>
#include <macros.hpp>

Lock::Version::Version(uint64_t _versionNumber, uint64_t _versionLock, uint64_t _lock) : 
                versionNumber(_versionNumber), versionLock(_versionLock), lock(_lock == reference) {}
Lock::Lock() : version(0) {}

bool Lock::acquire() {
    uint64_t _version = version.load(),
             // 011...111 & _version -> copy the version except first bit (lock)
             unlocked = bitMask & _version;
    
    /** The lock is represented by the first bit of the version
     *  1 -> taken, 0 -> not taken already
     */
    if (_version >> bigShift) {
        return false;
    }

    // Perform c&s in case of concurrent access to the lock after the check
    return compareAndSwap(true, unlocked, _version);
}

bool Lock::release() {
    uint64_t _version = version.load(),
             unlocked = bitMask & _version;

    // If the lock has already been released
    if (!(_version >> bigShift)) {
        return false;
    }
    
    // Perform c&s in case of concurrent access to the lock after the check
    return compareAndSwap(false, unlocked, _version);
}

bool Lock::setVersion(uint64_t newVersion) {
    uint64_t oldVersion = version.load();
    if (!(oldVersion >> bigShift)) {
        return false;
    }
    return compareAndSwap(false, newVersion, oldVersion);
}

Lock::Version Lock::sampleLock() {
    uint64_t _version = version.load();
    return Version(bitMask & _version, 
                    _version, 
                    _version >> bigShift);
}

bool Lock::compareAndSwap(bool lock, uint64_t newValue, uint64_t oldValue) {
    return version.compare_exchange_strong(oldValue, getVersion(lock, newValue));
}

uint64_t Lock::getVersion(bool lock, uint64_t newValue) {
    // Inglobe the lock bit in the version number
    if (lock) {
        return (reference << bigShift) | newValue;
    }
    return newValue;
}
