#pragma once

// Internal header
#include <macros.hpp>

/* Interface for acquiring and releasing the locks using CAS */

bool acquire(atomic_int*);
void release(atomic_int*);
bool setVersion(atomic_int*,int);