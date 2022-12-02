#pragma once

#include "lock.hpp"

class Word {
    public:
        uint64_t value;
        Lock lock;

        Word();

        Version sampleLock();
        bool acquire();
        void release();
        bool setVersion(uint64_t);
};

class Region {
    public:
        void *start;
        size_t size, align;
        vector<vector<Word>> matrix;

        Region(size_t,size_t);

        Word &getWord(tx_t);
        void *getAddress();

    private:
        atomic_uint64_t nextSegment;
        uint64_t getRow(tx_t);
        uint64_t getCol(tx_t);
};

