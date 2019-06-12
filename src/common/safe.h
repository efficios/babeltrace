
#include <stdbool.h>
#include <stdint.h>

static inline
bool bt_safe_to_mul_int64(int64_t a, int64_t b)
{
	if (a == 0 || b == 0) {
		return true;
	}

	return a < INT64_MAX / b;
}

static inline
bool bt_safe_to_mul_uint64(uint64_t a, uint64_t b)
{
	if (a == 0 || b == 0) {
		return true;
	}

	return a < UINT64_MAX / b;
}

static inline
bool bt_safe_to_add_int64(int64_t a, int64_t b)
{
	return a <= INT64_MAX - b;
}

static inline
bool bt_safe_to_add_uint64(uint64_t a, uint64_t b)
{
	return a <= UINT64_MAX - b;
}
