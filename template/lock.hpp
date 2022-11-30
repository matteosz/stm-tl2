#pragma once

#include <vector>
#include <atomic>
#include <unordered_set>
#include <unordered_map>
#include <cstring>

#include "macros.hpp"
#include "../include/tm.hpp"

class Version {
    public:
        uint64_t versionNumber, versionLock;
        bool lock;
        Version(uint64_t,uint64_t,uint64_t);
};

class Lock {
    public:
        std::atomic_uint64_t version;

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