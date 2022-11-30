#pragma once

#include <lock.hpp>
#include <cstdint>

class Word {
    public:
        uint64_t value;
        Lock lock;
        
        Word(uint64_t);
        Lock::Version sampleLock();
        bool acquire();
        void release();
        bool setVersion(uint64_t);
};