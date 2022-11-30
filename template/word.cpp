#include <word.hpp>

Word::Word(uint64_t _value) : value(_value) {}

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