#include "word.hpp"

Word::Word() : value(0) {}

Lock::Version Word::sampleLock() {
    return this->lock.sampleLock();
}

bool Word::acquire() {
    return this->lock.acquire();
}

void Word::release() {
    this->lock.release();
}

bool Word::setVersion(uint64_t newVersion) {
    return this->lock.setVersion(newVersion);
}