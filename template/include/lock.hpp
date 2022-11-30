#pragma once
#include "macros.hpp"
#include <atomic>
#include <cstdint>

using namespace std;

class Lock {
    public:
        class Version {
            public:
                uint64_t versionNumber, versionLock;
                bool lock;
                Version(uint64_t,uint64_t,bool);
        };

        atomic_uint64_t version;

        Lock();
        Lock(Lock*);

        bool acquire();
        bool release();
        bool setVersion(uint64_t);
        Version sampleLock();

    private:
        bool compareAndSwap(bool,uint64_t,uint64_t);
        uint64_t bloomFilter(bool,uint64_t);
};