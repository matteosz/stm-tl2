#pragma once

#include "lock.hpp"

class Word {
    public:
        uint64_t value;
        Lock lock;
        
        Word();
        Lock::Version sampleLock();
        bool acquire();
        void release();
        bool setVersion(uint64_t);
};