#include <region.hpp>

Word::Word() : value(0) {}

Version Word::sampleLock() {
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

Region::Region(size_t _size, size_t _align) : 
        start((void*) (reference << smallShift)), 
        size(_size), 
        align(_align), 
        matrix(m, vector<Word>(n)),
        globalClock(0), 
        nextSegment(2) {}

uint64_t Region::sampleClock() {
    return globalClock.load();
}

Word *Region::getWord(tx_t address) {
    return &matrix[getRow(address)][getCol(address)];
}

uint64_t Region::fetchAndIncSegments() {
    return nextSegment.fetch_add(1);
}

uint64_t Region::fetchAndIncClock() {
    return globalClock.fetch_add(1);
}

uint16_t Region::getRow(tx_t address) {
    return address >> smallShift;
}

uint16_t Region::getCol(tx_t address) {
    return ((address << smallShift) >> smallShift) / align;
}