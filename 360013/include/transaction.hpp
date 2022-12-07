#pragma once

// Internal header
#include <region.hpp>
#define COMMIT clean(); return true;
#define _ABORT clean(); return false;
#define ABORT transaction.clean(); return false; \

typedef struct {
    public:
        void *content, *raw;
        atomic_int *lock;
} WordItem;

class Transaction {
    public:
        bool readOnly; 
        int readVersion;
        unordered_set<atomic_int*> readSet;
        map<void*,WordItem> writeSet;
        vector<int> freeBuffer;
        
        Transaction();

        bool validate();
        bool commit(Region*,atomic_int*);
        void clean();
    
    private:
        bool acquireLocks(int*);
        void releaseLocks(int);
};