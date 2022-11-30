#pragma once

#include <atomic>

class Lock {
    public:
        class Version {
            public:
                uint64_t versionNumber, versionLock;
                bool lock;
                Version(uint64_t,uint64_t,uint64_t);
        };

        ::std::atomic_uint64_t version;

        Lock();

        bool acquire();
        bool release();
        bool setVersion(uint64_t);
        Version sampleLock();

    private:
        bool compareAndSwap(bool,uint64_t,uint64_t);
        uint64_t getVersion(bool,uint64_t);
};