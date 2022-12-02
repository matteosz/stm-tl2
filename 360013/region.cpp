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
        start((void*) firstAddress), 
        size(_size), 
        align(_align), 
        matrix(m, vector<Word>(n)),
        nextSegment(2) {}

Word &Region::getWord(tx_t address) {
    return matrix[getRow(address)][getCol(address)];
}

void Region::releaseMemory(tx_t address) {
    getWord(address).release();
}

void *Region::getAddress() {
    return (void*) (nextSegment.fetch_add(1) << shift);
}

uint32_t Region::getRow(tx_t address) {
    return address >> shift;
}

uint32_t Region::getCol(tx_t address) {
    return (uint32_t) address / align;
}