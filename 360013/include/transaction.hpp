#pragma once

#include <region.hpp>

class Transaction {
    public: 
        uint64_t rVersion, wVersion;
        unordered_set<void*> rSet;
        unordered_map<tx_t,void*> wSet;
        bool rOnly;
        
        Transaction(bool);

        void setRo(bool);
        void setClock(Region*);
        bool search(tx_t,void*,size_t);
        void insertReadSet(tx_t);
        void insertWriteSet(tx_t,void*);
        bool isEmpty();
        bool acquire(Region*,uint32_t*);
        void release(Region*,uint32_t);
        bool validate(Region*);
        void setWVersion(uint64_t);
        void clear();
        bool commit(Region*);
};