#include <region.hpp>

Word::Word() : value(0) {}

Region::Region(size_t _size, size_t _align) : 
        size(_size), 
        align(_align), 
        matrix(m, vector<Word>(n)),
        nextSegment(2) {}

Word &Region::getWord(tx_t address) {
    return matrix[address >> shift][(uint32_t) address / align];
}

void *Region::getAddress() {
    return (void*) (nextSegment.fetch_add(1) << shift);
}