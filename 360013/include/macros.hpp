#pragma once

#include <cstdint>

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

//#define DEBUG_

static constexpr uint16_t m = 450U, n = 800U;

static constexpr uint8_t longShift = 63U, shift = 32U;

// 0111...111, 1000...000
static constexpr uint64_t bitMask = (1ULL << 63U) - 1ULL, firstBitMask = 1ULL << 63U;

static constexpr intptr_t firstAddress = 0x1;
