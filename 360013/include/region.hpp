#pragma once

#include <lock.hpp>

class Word {
    public:
        uint64_t value;
        Lock lock;

        Word();
};

class Region {
    public:
        size_t size, align;
        vector<vector<Word>> matrix;

        Region(size_t,size_t);

        Word &getWord(tx_t);
        void *getAddress();

    private:
        atomic_uint64_t nextSegment;
};

