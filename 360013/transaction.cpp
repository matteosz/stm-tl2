#include <transaction.hpp>

Transaction::Transaction(bool ro) : rOnly(ro) {}

void Transaction::setClock(Region *region) {
    rVersion = region->sampleClock();
}

void Transaction::setRo(bool _rOnly) {
    rOnly = _rOnly;
}

bool Transaction::search(tx_t address, void* target, size_t align) {
    if (!rOnly) {
        auto search = wSet.find(address);
        // Already present in the write set
        if (search != wSet.end()) {
            memcpy(target, search->second, align);
            return true;
        }
    }
    return false;
}

void Transaction::insertReadSet(tx_t address) {
    if (!rOnly) {
        rSet.emplace((void*) address);
    }
}

void Transaction::insertWriteSet(tx_t target, void *source) {
    wSet[target] = source;
}

bool Transaction::isEmpty() {
    return wSet.empty();
}

bool Transaction::acquire(Region *region, uint32_t *count) {
    for (const auto &target : wSet) {
        Word &word = region->getWord(target.first);
        if (!word.acquire()) {
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
    for (const auto &target : wSet) {
        Word &word = region->getWord(target.first);
        word.release();
        if (count-- < 2) {
            break;
        }
    }
}

bool Transaction::validate(Region *region) {
    for (const auto address : rSet) {
        Word &word = region->getWord((uintptr_t) address);
        Version version = word.sampleLock();
        if (version.lock || version.versionNumber > rVersion) {
            return false;
        }
    }
    return true;
}

bool Transaction::commit(Region *region) {
    for (const auto &target : wSet) {
        Word &word = region->getWord(target.first);
        memcpy(&word.value, target.second, region->align);
        if (!word.setVersion(wVersion)) {
            clear();
            return false;
        }
    }
    clear();
    return true;
}