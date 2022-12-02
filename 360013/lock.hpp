#pragma once

#include "macros.hpp"

using namespace std;

class Version {
    public:
        uint64_t versionNumber, versionLock;
        bool lock;
        Version(uint64_t,uint64_t,uint64_t);
};

class Lock {
    public:
        atomic_uint64_t version;

        Lock();
        Lock(const Lock&);

        bool acquire();
        bool release();
        bool setVersion(uint64_t);
        Version sampleLock();

    private:
        bool compareAndSwap(bool,uint64_t,uint64_t);
        uint64_t getVersion(bool,uint64_t);
};