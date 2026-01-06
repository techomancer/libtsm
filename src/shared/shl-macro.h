/*
 * SHL - Macros and small helpers (IRIX-compatible version)
 *
 * Copyright (c) 2010-2013 David Herrmann <dh.herrmann@gmail.com>
 * Dedicated to the Public Domain
 *
 * IRIX Compatibility Note:
 * This version removes GCC statement expressions ({ }) which MIPSpro doesn't support.
 * We use regular functions instead of macros for overflow-safe multiplication.
 */

#ifndef SHL_MACRO_H
#define SHL_MACRO_H

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * Miscellaneous
 */

#define SHL_DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define SHL_EXPORT __attribute__((visibility("default")))
#define SHL_HAS_BITS(_bitmask, _bits) (((_bitmask) & (_bits)) == (_bits))

/* on 64bit, size_t is 8 bytes, otherwise 4 bytes */
#ifdef __LP64__
#define shl_assert_cc(_x) \
	do { \
		switch (0) { case 0: case (!!(_x)):; } \
	} while (0)
#else
/* MIPSpro n32 ABI: pointers are 4 bytes in -n32 mode */
#define shl_assert_cc(_x) \
	do { \
		switch (0) { case 0: case (!!(_x)):; } \
	} while (0)
#endif

#define shl_offsetof(_type, _member) ((size_t)&(((_type*)0)->_member))
#define shl_container_of(_ptr, _type, _member) \
	((_type*)(((char*)(_ptr)) - shl_offsetof(_type, _member)))

/*
 * Array/Object Management
 */

static inline void *shl_greedy_realloc(void **mem, size_t *size, size_t need)
{
	size_t nsize;
	void *p;

	if (!mem || !size)
		return NULL;

	if (*size >= need && *mem)
		return *mem;

	if (*size == 0)
		nsize = 1;
	else
		nsize = *size;

	while (nsize < need)
		nsize <<= 1;

	p = realloc(*mem, nsize);
	if (!p)
		return NULL;

	*mem = p;
	*size = nsize;
	return p;
}

/*
 * Round to next power of 2
 * Align to next higher power-of-2 (except for: 0 => 0, overflow => 0)
 */
static inline unsigned long long shl_next_power_of_2(unsigned long long u)
{
	unsigned long long v = 1;

	if (u == 0)
		return 1;

	/* Find next power of 2 using bit shifting */
	while (v < u && v != 0)
		v <<= 1;

	return v ? v : u;
}

#define SHL_ALIGN_POWER2(u) shl_next_power_of_2(u)

/*
 * Safe Multiplications
 * Multiplications are subject to overflows. These helpers guarantee that the
 * multiplication can be done safely and return -ERANGE if not.
 */

static inline int shl_mult_ull(unsigned long long *val,
			       unsigned long long factor)
{
	if (factor == 0 || *val <= ULLONG_MAX / factor) {
		*val *= factor;
		return 0;
	}
	return -ERANGE;
}

static inline int shl_mult_ul(unsigned long *val, unsigned long factor)
{
#if ULONG_MAX < ULLONG_MAX
	unsigned long long v = (unsigned long long)*val * (unsigned long long)factor;
	if (v <= ULONG_MAX) {
		*val = (unsigned long)v;
		return 0;
	}
	return -ERANGE;
#else
	return shl_mult_ull((unsigned long long*)val, factor);
#endif
}

static inline int shl_mult_u(unsigned int *val, unsigned int factor)
{
#if UINT_MAX < ULONG_MAX
	unsigned long v = (unsigned long)*val * (unsigned long)factor;
	if (v <= UINT_MAX) {
		*val = (unsigned int)v;
		return 0;
	}
	return -ERANGE;
#elif UINT_MAX < ULLONG_MAX
	unsigned long long v = (unsigned long long)*val * (unsigned long long)factor;
	if (v <= UINT_MAX) {
		*val = (unsigned int)v;
		return 0;
	}
	return -ERANGE;
#else
	return shl_mult_ull((unsigned long long*)val, factor);
#endif
}

static inline int shl_mult_u64(uint64_t *val, uint64_t factor)
{
	if (factor == 0 || *val <= UINT64_MAX / factor) {
		*val *= factor;
		return 0;
	}
	return -ERANGE;
}

static inline int shl_mult_u32(uint32_t *val, uint32_t factor)
{
	uint_fast64_t v = (uint_fast64_t)*val * (uint_fast64_t)factor;
	if (v <= UINT32_MAX) {
		*val = (uint32_t)v;
		return 0;
	}
	return -ERANGE;
}

static inline int shl_mult_u16(uint16_t *val, uint16_t factor)
{
	uint_fast32_t v = (uint_fast32_t)*val * (uint_fast32_t)factor;
	if (v <= UINT16_MAX) {
		*val = (uint16_t)v;
		return 0;
	}
	return -ERANGE;
}

static inline int shl_mult_u8(uint8_t *val, uint8_t factor)
{
	uint_fast16_t v = (uint_fast16_t)*val * (uint_fast16_t)factor;
	if (v <= UINT8_MAX) {
		*val = (uint8_t)v;
		return 0;
	}
	return -ERANGE;
}

#endif  /* SHL_MACRO_H */
