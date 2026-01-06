/* SPDX-License-Identifier: MIT */
/*
 * Minimal Check unit testing framework stub for IRIX
 *
 * This is a minimal implementation of the Check API used by libtsm tests.
 * It provides just enough functionality to compile and run the tests without
 * requiring the full Check library to be ported to IRIX.
 *
 * This is NOT a complete Check implementation - only what libtsm needs.
 */

#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* MIPSpro compatibility - use __inline instead of inline */
#ifdef __sgi
#define INLINE __inline
#else
#define INLINE inline
#endif

/* Test result tracking */
static int check_n_tests = 0;
static int check_n_failed = 0;
static const char *check_current_test = NULL;

/* Opaque types */
typedef struct TCase TCase;
typedef struct Suite Suite;
typedef struct SRunner SRunner;

struct TCase {
	const char *name;
	void (*setup)(void);
	void (*teardown)(void);
	void (**tests)(void);
	int n_tests;
	int capacity;
};

struct Suite {
	const char *name;
	TCase **cases;
	int n_cases;
	int capacity;
};

struct SRunner {
	Suite *suite;
};

/* Test runner modes */
typedef enum {
	CK_NORMAL = 0,
	CK_VERBOSE = 1
} CheckMode;

/* Helper macros */
#define START_TEST(name) \
	static void name(void)

#define END_TEST

/* Assertion helper function */
static void _ck_assert_failed(const char *file, int line, const char *msg) {
	fprintf(stderr, "FAIL: %s:%d: %s\n", file, line, msg);
	check_n_failed++;
}

/* MIPSpro-compatible ck_assert using ternary operator like MIPSpro's assert */
#define ck_assert(EX) ((EX) ? ((void)0) : _ck_assert_failed(__FILE__, __LINE__, #EX))

#define ck_assert_int_eq(a, b) do { \
	int _a = (a); \
	int _b = (b); \
	if (_a != _b) { \
		char buf[256]; \
		sprintf(buf, "%s == %s (%d != %d)", #a, #b, _a, _b); \
		_ck_assert_failed(__FILE__, __LINE__, buf); \
	} \
} while (0)

#define ck_assert_int_ne(a, b) do { \
	int _a = (a); \
	int _b = (b); \
	if (_a == _b) { \
		char buf[256]; \
		sprintf(buf, "%s != %s (%d == %d)", #a, #b, _a, _b); \
		_ck_assert_failed(__FILE__, __LINE__, buf); \
	} \
} while (0)

#define ck_assert_int_lt(a, b) do { \
	int _a = (a); \
	int _b = (b); \
	if (_a >= _b) { \
		char buf[256]; \
		sprintf(buf, "%s < %s (%d >= %d)", #a, #b, _a, _b); \
		_ck_assert_failed(__FILE__, __LINE__, buf); \
	} \
} while (0)

#define ck_assert_int_le(a, b) do { \
	int _a = (a); \
	int _b = (b); \
	if (_a > _b) { \
		char buf[256]; \
		sprintf(buf, "%s <= %s (%d > %d)", #a, #b, _a, _b); \
		_ck_assert_failed(__FILE__, __LINE__, buf); \
	} \
} while (0)

#define ck_assert_int_gt(a, b) do { \
	int _a = (a); \
	int _b = (b); \
	if (_a <= _b) { \
		char buf[256]; \
		sprintf(buf, "%s > %s (%d <= %d)", #a, #b, _a, _b); \
		_ck_assert_failed(__FILE__, __LINE__, buf); \
	} \
} while (0)

#define ck_assert_int_ge(a, b) do { \
	int _a = (a); \
	int _b = (b); \
	if (_a < _b) { \
		char buf[256]; \
		sprintf(buf, "%s >= %s (%d < %d)", #a, #b, _a, _b); \
		_ck_assert_failed(__FILE__, __LINE__, buf); \
	} \
} while (0)

#define ck_assert_uint_eq(a, b) do { \
	unsigned int _a = (a); \
	unsigned int _b = (b); \
	if (_a != _b) { \
		char buf[256]; \
		sprintf(buf, "%s == %s (%u != %u)", #a, #b, _a, _b); \
		_ck_assert_failed(__FILE__, __LINE__, buf); \
	} \
} while (0)

#define ck_assert_uint_ne(a, b) do { \
	unsigned int _a = (a); \
	unsigned int _b = (b); \
	if (_a == _b) { \
		char buf[256]; \
		sprintf(buf, "%s != %s (%u == %u)", #a, #b, _a, _b); \
		_ck_assert_failed(__FILE__, __LINE__, buf); \
	} \
} while (0)

#define ck_assert_ptr_eq(a, b) do { \
	void *_a = (void*)(a); \
	void *_b = (void*)(b); \
	if (_a != _b) { \
		char buf[256]; \
		sprintf(buf, "%s == %s (%p != %p)", #a, #b, _a, _b); \
		_ck_assert_failed(__FILE__, __LINE__, buf); \
	} \
} while (0)

#define ck_assert_ptr_ne(a, b) do { \
	void *_a = (void*)(a); \
	void *_b = (void*)(b); \
	if (_a == _b) { \
		char buf[256]; \
		sprintf(buf, "%s != %s (%p == %p)", #a, #b, _a, _b); \
		_ck_assert_failed(__FILE__, __LINE__, buf); \
	} \
} while (0)

#define ck_assert_str_eq(a, b) do { \
	const char *_a = (a); \
	const char *_b = (b); \
	if (strcmp(_a, _b) != 0) { \
		char buf[512]; \
		sprintf(buf, "%s == %s (\"%s\" != \"%s\")", #a, #b, _a, _b); \
		_ck_assert_failed(__FILE__, __LINE__, buf); \
	} \
} while (0)

#define ck_assert_str_ne(a, b) do { \
	const char *_a = (a); \
	const char *_b = (b); \
	if (strcmp(_a, _b) == 0) { \
		char buf[512]; \
		sprintf(buf, "%s != %s (\"%s\" == \"%s\")", #a, #b, _a, _b); \
		_ck_assert_failed(__FILE__, __LINE__, buf); \
	} \
} while (0)

#define ck_assert_uint_gt(a, b) do { \
	unsigned int _a = (a); \
	unsigned int _b = (b); \
	if (_a <= _b) { \
		char buf[256]; \
		sprintf(buf, "%s > %s (%u <= %u)", #a, #b, _a, _b); \
		_ck_assert_failed(__FILE__, __LINE__, buf); \
	} \
} while (0)

#define ck_assert_mem_eq(a, b, len) do { \
	const void *_a = (a); \
	const void *_b = (b); \
	size_t _len = (len); \
	if (memcmp(_a, _b, _len) != 0) { \
		char buf[256]; \
		sprintf(buf, "%s == %s (memory differs)", #a, #b); \
		_ck_assert_failed(__FILE__, __LINE__, buf); \
	} \
} while (0)

/* Test case management */
static INLINE TCase *tcase_create(const char *name)
{
	TCase *tc = (TCase*)malloc(sizeof(TCase));
	tc->name = name;
	tc->setup = NULL;
	tc->teardown = NULL;
	tc->tests = NULL;
	tc->n_tests = 0;
	tc->capacity = 0;
	return tc;
}

static INLINE void tcase_add_test(TCase *tc, void (*test)(void))
{
	if (tc->n_tests >= tc->capacity) {
		tc->capacity = tc->capacity == 0 ? 8 : tc->capacity * 2;
		tc->tests = (void (**)(void))realloc(tc->tests,
			tc->capacity * sizeof(void (*)(void)));
	}
	tc->tests[tc->n_tests++] = test;
}

static INLINE void tcase_add_checked_fixture(TCase *tc,
	void (*setup)(void), void (*teardown)(void))
{
	tc->setup = setup;
	tc->teardown = teardown;
}

static INLINE void tcase_add_unchecked_fixture(TCase *tc,
	void (*setup)(void), void (*teardown)(void))
{
	tc->setup = setup;
	tc->teardown = teardown;
}

/* Suite management */
static INLINE Suite *suite_create(const char *name)
{
	Suite *s = (Suite*)malloc(sizeof(Suite));
	s->name = name;
	s->cases = NULL;
	s->n_cases = 0;
	s->capacity = 0;
	return s;
}

static INLINE void suite_add_tcase(Suite *s, TCase *tc)
{
	if (s->n_cases >= s->capacity) {
		s->capacity = s->capacity == 0 ? 8 : s->capacity * 2;
		s->cases = (TCase**)realloc(s->cases, s->capacity * sizeof(TCase*));
	}
	s->cases[s->n_cases++] = tc;
}

/* Suite runner */
static INLINE SRunner *srunner_create(Suite *s)
{
	SRunner *sr = (SRunner*)malloc(sizeof(SRunner));
	sr->suite = s;
	return sr;
}

static INLINE void srunner_run_all(SRunner *sr, CheckMode mode)
{
	Suite *s = sr->suite;
	int i, j;

	printf("Running suite: %s\n", s->name);
	check_n_tests = 0;
	check_n_failed = 0;

	for (i = 0; i < s->n_cases; i++) {
		TCase *tc = s->cases[i];
		printf("  Test case: %s\n", tc->name);

		for (j = 0; j < tc->n_tests; j++) {
			int failed_before = check_n_failed;

			if (tc->setup)
				tc->setup();

			check_n_tests++;
			tc->tests[j]();

			if (tc->teardown)
				tc->teardown();

			if (check_n_failed == failed_before) {
				if (mode == CK_VERBOSE)
					printf("    [PASS]\n");
			}
		}
	}

	printf("\n");
	printf("Results: %d tests, %d passed, %d failed\n",
		check_n_tests, check_n_tests - check_n_failed, check_n_failed);
}

static INLINE int srunner_ntests_failed(SRunner *sr)
{
	return check_n_failed;
}

static INLINE void srunner_free(SRunner *sr)
{
	Suite *s = sr->suite;
	int i;

	for (i = 0; i < s->n_cases; i++) {
		TCase *tc = s->cases[i];
		if (tc->tests)
			free(tc->tests);
		free(tc);
	}
	if (s->cases)
		free(s->cases);
	free(s);
	free(sr);
}

#endif /* CHECK_H */
