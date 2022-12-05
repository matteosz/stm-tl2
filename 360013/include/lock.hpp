#pragma once

// Internal header
#include <macros.hpp>

/* Interface for acquiring and releasing the locks using CAS */

bool acquire(atomic_int*,int);
int getVersion(atomic_int*);
void removeLock(atomic_int*);
//void release(atomic_int*);
void update(atomic_int*,int);