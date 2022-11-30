#include <transaction.hpp>

Transaction::Transaction(bool ro) : rOnly(ro) {}

void Transaction::setClock(Region *region) {
    rVersion = region->sampleClock();
}

void Transaction::setRo(bool _rOnly) {
    rOnly = _rOnly;
}

unordered_map<tx_t, void*>::iterator Transaction::search(tx_t address) {
    return wSet.find(address);
}

unordered_map<tx_t, void*>::iterator Transaction::wEnd() {
    return wSet.end();
}

void Transaction::insertReadSet(tx_t address) {
    rSet.insert((void*) address);
}

void Transaction::insertWriteSet(tx_t target, void *source) {
    wSet[target] = source;
}

bool Transaction::isEmpty() {
    return wSet.empty();
}

bool Transaction::acquire(Region *region, uint32_t *count) {
    for (auto &target : wSet) {
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
    wVersion = newVersion + 1;
}

void Transaction::clear() {
    for (auto &address : rSet) {
        free(address);
    }
    for (auto &address : wSet) {
        free(address.second);
    }
    this->rSet.clear();
    this->wSet.clear();
    this->rVersion = 0;
    this->rOnly = false;
}

void Transaction::release(Region *region, uint32_t count) {
    if (!count) {
        return;
    }
    for (auto &target : wSet) {
        Word *word = region->getWord(target.first);
        word->release();
        if (count-- < 2) {
            break;
        }
    }
}

bool Transaction::validate(Region *region) {
    for (auto address : rSet) {
        Word *word = region->getWord((uintptr_t) address);
        Lock::Version version = word->sampleLock();
        if (version.lock || version.versionNumber > rVersion) {
            return false;
        }
    }
    return true;
}

bool Transaction::commit(Region *region) {
    for (auto &target : wSet) {
        Word *word = region->getWord(target.first);
        memcpy(&word->value, target.second, region->align);
        if (!word->setVersion(wVersion)) {
            clear();
            return false;
        }
    }
    clear();
    return true;
}