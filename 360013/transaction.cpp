#include <transaction.hpp>

Transaction::Transaction() : rOnly(false) {}

bool Transaction::search(tx_t address, void* target, size_t align) {
    if (!rOnly) {
        auto search = wSet.find(address);
        // If already present in the write set
        if (search != wSet.end()) {
            #ifdef _DEBUG_
                cout << "Address found on write set, just copying\n";
            #endif
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
        if (!word.lock.acquire()) {
            release(region, *count);
            return false;
        }
        ++*count;
    }
    return true;
}

void Transaction::setWVersion(atomic_uint64_t *clock) {
    wVersion = clock->fetch_add(1) + 1;
}

void Transaction::setRVersion(uint64_t newVersion) {
    rVersion = newVersion;
}

void Transaction::clear() {
    for (const auto &address : wSet) {
        free(address.second);
    }
    rVersion = 0;
    rOnly = false;
    rSet.clear();
    wSet.clear();
}

void Transaction::release(Region *region, uint32_t count) {
    if (!count) {
        return;
    }
    for (const auto &target : wSet) {
        region->getWord(target.first).lock.release();
        if (count == 1) {
            // Don't free the first segment
            break;
        }
        --count;
    }
}

bool Transaction::validate(Region *region) {
    for (const auto address : rSet) {
        if (!region->getWord((tx_t) address).lock.sampleLock().valid(rVersion)) {
            return false;
        }
    }
    return true;
}

bool Transaction::commit(Region *region) {
    for (const auto target : wSet) {
        Word &word = region->getWord(target.first);
        memcpy(&word.value, target.second, region->align);
        if (!word.lock.setVersion(wVersion)) {
            clear();
            #ifdef _DEBUG_
                cout << "Failed to commit\n";
            #endif
            return false;
        }
    }
    clear();
    #ifdef _DEBUG_
        cout << "Committed correctly\n";
    #endif
    return true;
}