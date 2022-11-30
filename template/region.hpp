#pragma once

#include <vector>
#include <atomic>
#include "tm.hpp"
#include "macros.hpp"
#include "word.hpp"

using namespace std;

class Region {
    public:
        void *start;
        size_t size, align;
        atomic_uint64_t globalClock, nextSegment;
        vector<vector<Word>> matrix;

        Region(size_t,size_t);

        uint64_t sampleClock();
        Word* getWord(tx_t);
        uint64_t incrementSegments();
        uint64_t incrementClock();

    private:
        uint16_t getRow(tx_t);
        uint16_t getCol(tx_t);
};