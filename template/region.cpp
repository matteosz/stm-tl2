#include "region.hpp"

Region::Region(size_t size, size_t align) : 
        start((void*) (reference << bitShift)), 
        size(size), 
        align(align), 
        nextSegment(2), 
        matrix(m, vector<Word>(n)) {}

uint64_t Region::sampleClock() {
    return this->globalClock.load();
}
Word* Region::getWord(tx_t address) {
    return &(this->matrix[getRow(address)][getCol(address)]);
}
uint64_t Region::incrementSegments() {
    return this->nextSegment.fetch_add(1);
}
uint64_t Region::incrementClock() {
    return this->globalClock.fetch_add(1);
}

uint16_t Region::getRow(tx_t address) {
    return address >> bitShift;
}
uint16_t Region::getCol(tx_t address) {
    return ((address << bitShift) >> bitShift) / this->align;
}