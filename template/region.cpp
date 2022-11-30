#include "region.hpp"

Region::Region(size_t _size, size_t _align) : 
        start(startAddress), 
        size(_size), 
        align(_align), 
        nextSegment(2), 
        matrix(m, vector<Word>(n)) {}

uint64_t Region::sampleClock() {
    return globalClock.load();
}

Word *Region::getWord(tx_t address) {
    return &(matrix[getRow(address)][getCol(address)]);
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