#include "word.hpp"

Word::Word() : value(0) {}

Lock::Version Word::sampleLock() {
    return lock.sampleLock();
}

bool Word::acquire() {
    return lock.acquire();
}

void Word::release() {
    lock.release();
}

bool Word::setVersion(uint64_t newVersion) {
    return lock.setVersion(newVersion);
}