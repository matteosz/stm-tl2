#include "transaction.hpp"

void Transaction::setClock(Region *region) {
    this->rVersion = region->sampleClock();
}

void Transaction::setRo(bool rOnly) {
    this->rOnly = rOnly;
}

unordered_map<tx_t, void*>::iterator Transaction::search(tx_t address) {
    return this->wSet.find(address);
}

unordered_map<tx_t, void*>::iterator Transaction::wEnd() {
    return this->wSet.end();
}

void Transaction::insertReadSet(tx_t address) {
    this->rSet.insert((void*) address);
}

void Transaction::insertWriteSet(tx_t target, void* source) {
    this->wSet[target] = source;
}

bool Transaction::isEmpty() {
    return this->wSet.empty();
}

bool Transaction::acquire(Region* region, uint32_t* count) {
    for (auto &target : this->wSet) {
        Word *word = region->getWord(target.first);
        if (!word->acquire()) {
            release(region, *count);
            return false;
        }
        (*count)++;
    }
    return true;
}

void Transaction::setWVersion(uint64_t newVersion) {
    this->wVersion = newVersion + 1;
}

void Transaction::clear() {
    for (auto& address : this->rSet) {
        free(address);
    }
    for (auto& address : this->wSet) {
        free(address.second);
    }
    this->rSet.clear();
    this->wSet.clear();
    this->rVersion = 0;
    this->rOnly = false;
}

void Transaction::release(Region* region, uint32_t count) {
    if (count == 0) {
        return;
    }
    for (auto &target : this->wSet) {
        Word *word = region->getWord(target.first);
        word->release();
        if (count < 2) {
            break;
        }
        --count;
    }
}

bool Transaction::validate(Region* region) {
    for (auto address : this->rSet) {
        Word *word = region->getWord((uintptr_t) address);
        Lock::Version version = word->sampleLock();
        if (version.lock || version.versionNumber > this->rVersion) {
            return false;
        }
    }
  return true;
}

bool Transaction::commit(Region *region) {
    for (auto &target : this->wSet) {
        Word *word = region->getWord(target.first);
        memcpy(&word->value, target.second, region->align);
        if (!word->setVersion(this->wVersion)) {
            clear();
            return false;
        }
    }

    clear();
    return true;
}