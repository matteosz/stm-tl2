#pragma once

// Internal header
#include <region.hpp>

// Shorthand defines
#define TX Transaction *transaction = (Transaction*) tx;      
#define ABORT transaction->clean(region, true); return false; 
#define _ABORT clean(region, true); return false;             
#define COMMIT clean(region, false); return true;             \

class Transaction {
    public:
        bool readOnly; 
        int readVersion;
        unordered_set<atomic_int*> readSet;
        map<void*,void*> writeSet;
        vector<int> freeBuffer;
        
        Transaction(bool,Region*);

        bool validate();
        bool pushRead(atomic_int*);
        void clean(Region*,bool);
        bool commit(Region*);

        ~Transaction();
    
    private:
        bool acquireLocks(Region*,int*);
        void releaseLocks(Region*,int);
};