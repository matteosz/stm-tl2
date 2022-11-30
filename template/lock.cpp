#include "lock.hpp"

#include <cstdint>
#include "macros.hpp"

Lock::Version::Version(uint64_t versionNumber, uint64_t versionLock, bool lock) : 
                versionNumber(versionNumber), versionLock(versionLock), lock(lock) {}
Lock::Lock() : version(0) {}
Lock::Lock(Lock *other) : version(other->version.load()) {}

bool Lock::acquire() {
    uint64_t bitString = this->version.load(),
             newVersion = ((reference << bitMask) - reference) & bitString;
    bool lock = (bitString >> bitMask) == reference;
    // If the lock has already been taken
    if (lock) {
        return false;
    }
    // Perform c&s in case of concurrent access to the lock
    return compareAndSwap(true, newVersion, bitString);
}

bool Lock::release() {
    uint64_t bitString = this->version.load(),
             newVersion = ((reference << bitMask) - reference) & bitString;
    bool lock = (bitString >> bitMask) == reference;
    // If the lock has already been released
    if (!lock) {
        return false;
    }
    // Perform c&s in case of concurrent access to the lock
    return compareAndSwap(false, newVersion, bitString);
}

bool Lock::setVersion(uint64_t newVersion) {
    uint64_t bitString = this->version.load();
    bool lock = (bitString >> bitMask) == reference;
    // If the lock has already been released
    if (!lock) {
        return false;
    }
    // Perform c&s in case of concurrent access to the lock
    return compareAndSwap(false, newVersion, bitString);
}

Lock::Version Lock::sampleLock() {
    uint64_t bitString = this->version.load(),
             newVersion = ((reference << bitMask) - reference) & bitString;
    bool lock = (bitString >> bitMask) == reference;
    return Version(newVersion, bitString, lock);
}

bool Lock::compareAndSwap(bool lock, uint64_t newValue, uint64_t oldValue) {
    return this->version.compare_exchange_strong(oldValue, getVersion(lock, newValue));
}

uint64_t Lock::getVersion(bool lock, uint64_t newValue) {
    if (lock) {
        return (reference << bitMask) | newValue;
    }
    return newValue;
}
