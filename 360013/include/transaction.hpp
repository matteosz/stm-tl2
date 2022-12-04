#pragma once

// Internal header
#include <region.hpp>

// Shorthand macros
#define TX Transaction *transaction = (Transaction*) tx;      
#define ABORT transaction->failed=true;delete transaction; return false; 
#define _ABORT failed = true;delete this; return false;             
#define COMMIT delete this; return true;

class Transaction {
    public:
        bool readOnly; 
        int readVersion;
        unordered_set<atomic_int*> readSet;
        map<void*,void*> writeSet;
        vector<int> freeBuffer;
        Region *region;
        bool failed;
        
        Transaction(bool,Region*);

        bool validate();
        bool commit();

        ~Transaction();
    
    private:
        bool acquireLocks(int*);
        void releaseLocks(int);
};