#pragma once

#include <vector>
#include <atomic>
#include <unordered_set>
#include <map>
#include <cstring>
#include <iostream>
#include <bitset>

#include "macros.hpp"
#include "../../include/tm.hpp"

using namespace std;

class Version {
    public:
        uint64_t versionNumber, versionLock;
        bool lock;
        Version(uint64_t,uint64_t,bool);

        bool valid(uint64_t);
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
};