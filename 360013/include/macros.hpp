#pragma once

// External headers
#include <cstdint>
#include <vector>
#include <queue>
#include <atomic>
#include <unordered_set>
#include <map>
#include <cstring>
#include <iostream>
#include <bitset>
#include <pthread.h>

// Internal header
#include "../../include/tm.hpp"

// Namespace
using namespace std;

// To activate debug mode
#define _DEBUG_

/* MACROS */

// Virtual address translation
#define index(x) ((((size_t) x >> 48) & 0xFFFF) - 1)
#define offset(x) (size_t) ((size_t) x & 0xFFFFFFFFFFFF)
#define address(x) (void*) (((size_t) (x+1) << 48))

// By assuming x is an integer, if it's locked (MSB=1), then it's negative
#define isLocked(x) (x < 0)

// Clear MSB using a mask
#define unlock(x) (x & 0x7FFFFFFF)
// Set to 1 MSB using a mask
#define addLock(x) (x | 0x80000000)

/* CONSTANTS */

// Threshold to start freeing the transaction alloc segments
#define BATCH 64

// Number of preallocated segments = 2^16
#define MAXSEGMENTS 0x10000

// Base of virtual address = 2^48
#define START_ADDRESS (void*) 0x1000000000000

// Requested features
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L
#ifdef __STDC_NO_ATOMICS__
    #error Current C11 compiler does not support atomic operations
#endif

/** Define a proposition as likely true.
 * @param prop Proposition
**/
#undef likely
#ifdef __GNUC__
    #define likely(prop) \
        __builtin_expect((prop) ? true : false, true /* likely */)
#else
    #define likely(prop) \
        (prop)
#endif

/** Define a proposition as likely false.
 * @param prop Proposition
**/
#undef unlikely
#ifdef __GNUC__
    #define unlikely(prop) \
        __builtin_expect((prop) ? true : false, false /* unlikely */)
#else
    #define unlikely(prop) \
        (prop)
#endif

/** Define a variable as unused.
**/
#undef unused
#ifdef __GNUC__
    #define unused(variable) \
        variable __attribute__((unused))
#else
    #define unused(variable)
    #warning This compiler has no support for GCC attributes
#endif