#include "rts_allocator.h"
#include <assert.h>
#include <limits.h>

// may be changed to custom error handling system
#ifndef RTSALLOC_ASSERT
	#define RTSALLOC_ASSERT(x) assert(x)
#endif

#ifndef RTSALLOC_USE_INTRINSICS
	#define RTSALLOC_USE_INTRINSICS 1
#endif

#if RTSALLOC_USE_INTRINSICS && !defined(RTSALLOC_LIKELY)
	#if defined(__GNUC__) || defined(__clang__) || defined(__CC_ARM)
		#define RTSALLOC_LIKELY(x) __builtin_expect((x), 1)
	#endif
#endif
#ifndef RTSALLOC_LIKELY
	#define RTSALLOC_LIKELY(x) x
#endif

#ifndef RTSALLOC_API
	#if _DEBUG
		#define RTSALLOC_API static inline
	#else
		#define RTSALLOC_API inline
	#endif
#endif

#if RTSALLOC_USE_INTRINSICS && !defined(RTSALLOC_CLZ)
	#if defined(__GNUC__) || defined(__clang__) || defined(__CC_ARM)
		#define RTSALLOC_CLZ __builtin_clzl
	#endif
#endif
#ifndef RTSALLOC_CLZ
RTSALLOC_API uint_fast8_t RTSALLOC_CLZ(const size_t x)
{
	RTSALLOC_ASSERT(x > 0);
	size_t       t = ((size_t)1U) << ((sizeof(size_t) * CHAR_BIT) - 1U);
	uint_fast8_t r = 0;
	while ((x & t) == 0)
	{
		t >>= 1U;
		r++;
	}
	return r;
}
#endif

// At least C99 is required
#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 199901L)
	#error "Unsupported language: ISO C99 or a newer version is required."
#endif

// static_assert for C99
#if __STDC_VERSION__ < 201112L
	#define static_assert(x, ...) typedef char _static_assert_gl(_static_assertion_, __LINE__)[(x) ? 1 : -1]
	#define _static_assert_gl(a, b) _static_assert_gl_impl(a, b)
	#define _static_assert_gl_impl(a, b) a##b 
#endif

#define FRAGMENT_SIZE_MIN (RTSALLOC_ALIGNMENT * 2U)
#define FRAGMENT_SIZE_MAX ((SIZE_MAX >> 1U) + 1U)
#define NUM_BINS_MAX (sizeof(size_t) * CHAR_BIT)

static_assert((RTSALLOC_ALIGNMENT& (RTSALLOC_ALIGNMENT - 1U)) == 0U, "Not a power of 2");
static_assert((FRAGMENT_SIZE_MIN& (FRAGMENT_SIZE_MIN - 1U)) == 0U, "Not a power of 2");
static_assert((FRAGMENT_SIZE_MAX& (FRAGMENT_SIZE_MAX - 1U)) == 0U, "Not a power of 2");

typedef struct Fragment Fragment;

typedef struct FragmentHeader
{
	Fragment* next;
	Fragment* prev;
	size_t    size;
	bool      used;
} FragmentHeader;
static_assert(sizeof(FragmentHeader) <= RTSALLOC_ALIGNMENT, "Memory layout error");

struct Fragment
{
	FragmentHeader header;
	Fragment* next_free;  // NULL == last
	Fragment* prev_free;  // NULL == first
};
static_assert(sizeof(Fragment) <= FRAGMENT_SIZE_MIN, "Memory layout error");

struct RTSAllocInstance
{
	Fragment* bins[NUM_BINS_MAX];
	size_t    nonempty_bin_mask;
	RTSAllocDiagnostics diagnostics;
};

// memory must be allocated with alignment
#define INSTANCE_SIZE_PADDED ((sizeof(RTSAllocInstance) + RTSALLOC_ALIGNMENT - 1U) & ~(RTSALLOC_ALIGNMENT - 1U))

static_assert(INSTANCE_SIZE_PADDED >= sizeof(RTSAllocInstance), "Invalid instance footprint computation");
static_assert((INSTANCE_SIZE_PADDED% RTSALLOC_ALIGNMENT) == 0U, "Invalid instance footprint computation");

RTSALLOC_API uint_fast8_t log2Floor(const size_t x)
{
	RTSALLOC_ASSERT(x > 0);
	return (uint_fast8_t)(((sizeof(x) * CHAR_BIT) - 1U) - ((uint_fast8_t)RTSALLOC_CLZ(x)));
}

// Special case: if the argument is zero, returns zero.
RTSALLOC_API uint_fast8_t log2Ceil(const size_t x)
{
	return (x <= 1U) ? 0U : (uint_fast8_t)((sizeof(x) * CHAR_BIT) - ((uint_fast8_t)RTSALLOC_CLZ(x - 1U)));
}

RTSALLOC_API size_t pow2(const uint_fast8_t power)
{
	return ((size_t)1U) << power;
}

RTSALLOC_API size_t roundUpToPowerOf2(const size_t x)
{
	RTSALLOC_ASSERT(x >= 2U);
	return ((size_t)1U) << ((sizeof(x) * CHAR_BIT) - ((uint_fast8_t)RTSALLOC_CLZ(x - 1U)));
}

RTSALLOC_API void interlink(Fragment* const left, Fragment* const right)
{
	if (RTSALLOC_LIKELY(left != NULL))
	{
		left->header.next = right;
	}
	if (RTSALLOC_LIKELY(right != NULL))
	{
		right->header.prev = left;
	}
}

RTSALLOC_API void rebin(RTSAllocInstance* const handle, Fragment* const fragment)
{
	RTSALLOC_ASSERT(handle != NULL);
	RTSALLOC_ASSERT(fragment != NULL);
	RTSALLOC_ASSERT(fragment->header.size >= FRAGMENT_SIZE_MIN);
	RTSALLOC_ASSERT((fragment->header.size % FRAGMENT_SIZE_MIN) == 0U);
	const uint_fast8_t idx = log2Floor(fragment->header.size / FRAGMENT_SIZE_MIN);
	RTSALLOC_ASSERT(idx < NUM_BINS_MAX);

	fragment->next_free = handle->bins[idx];
	fragment->prev_free = NULL;
	if (RTSALLOC_LIKELY(handle->bins[idx] != NULL))
	{
		handle->bins[idx]->prev_free = fragment;
	}
	handle->bins[idx] = fragment;
	handle->nonempty_bin_mask |= pow2(idx);
}

RTSALLOC_API void unbin(RTSAllocInstance* const handle, const Fragment* const fragment)
{
	RTSALLOC_ASSERT(handle != NULL);
	RTSALLOC_ASSERT(fragment != NULL);
	RTSALLOC_ASSERT(fragment->header.size >= FRAGMENT_SIZE_MIN);
	RTSALLOC_ASSERT((fragment->header.size % FRAGMENT_SIZE_MIN) == 0U);
	const uint_fast8_t idx = log2Floor(fragment->header.size / FRAGMENT_SIZE_MIN);  // Round DOWN when removing.
	RTSALLOC_ASSERT(idx < NUM_BINS_MAX);

	if (RTSALLOC_LIKELY(fragment->next_free != NULL))
	{
		fragment->next_free->prev_free = fragment->prev_free;
	}
	if (RTSALLOC_LIKELY(fragment->prev_free != NULL))
	{
		fragment->prev_free->next_free = fragment->next_free;
	}

	if (RTSALLOC_LIKELY(handle->bins[idx] == fragment))
	{
		RTSALLOC_ASSERT(fragment->prev_free == NULL);
		handle->bins[idx] = fragment->next_free;
		if (RTSALLOC_LIKELY(handle->bins[idx] == NULL))
		{
			handle->nonempty_bin_mask &= ~pow2(idx);
		}
	}
}

RTSAllocInstance* RTSAllocInit(void* const base, const size_t size)
{
	RTSAllocInstance* out = NULL;
	if ((base != NULL) && ((((size_t)base) % RTSALLOC_ALIGNMENT) == 0U) &&
		(size >= (INSTANCE_SIZE_PADDED + FRAGMENT_SIZE_MIN)))
	{
		RTSALLOC_ASSERT(((size_t)base) % sizeof(RTSAllocInstance*) == 0U);
		out = (RTSAllocInstance*)base;
		out->nonempty_bin_mask = 0U;
		for (size_t i = 0; i < NUM_BINS_MAX; i++)
		{
			out->bins[i] = NULL;
		}

		size_t capacity = size - INSTANCE_SIZE_PADDED;
		if (capacity > FRAGMENT_SIZE_MAX)
		{
			capacity = FRAGMENT_SIZE_MAX;
		}
		while ((capacity % FRAGMENT_SIZE_MIN) != 0)
		{
			RTSALLOC_ASSERT(capacity > 0U);
			capacity--;
		}
		RTSALLOC_ASSERT((capacity % FRAGMENT_SIZE_MIN) == 0);
		RTSALLOC_ASSERT((capacity >= FRAGMENT_SIZE_MIN) && (capacity <= FRAGMENT_SIZE_MAX));

		Fragment* const frag = (Fragment*)(void*)(((char*)base) + INSTANCE_SIZE_PADDED);
		RTSALLOC_ASSERT((((size_t)frag) % RTSALLOC_ALIGNMENT) == 0U);
		frag->header.next = NULL;
		frag->header.prev = NULL;
		frag->header.size = capacity;
		frag->header.used = false;
		frag->next_free = NULL;
		frag->prev_free = NULL;
		rebin(out, frag);
		RTSALLOC_ASSERT(out->nonempty_bin_mask != 0U);

		out->diagnostics.capacity = capacity;
		out->diagnostics.allocated = 0U;
		out->diagnostics.peak_allocated = 0U;
		out->diagnostics.peak_request_size = 0U;
		out->diagnostics.oom_count = 0U;
	}

	return out;
}

void* RTSAllocAllocate(RTSAllocInstance* const handle, const size_t amount)
{
	RTSALLOC_ASSERT(handle != NULL);
	RTSALLOC_ASSERT(handle->diagnostics.capacity <= FRAGMENT_SIZE_MAX);
	void* out = NULL;

	if (RTSALLOC_LIKELY((amount > 0U) && (amount <= (handle->diagnostics.capacity - RTSALLOC_ALIGNMENT))))
	{
		const size_t fragment_size = roundUpToPowerOf2(amount + RTSALLOC_ALIGNMENT);
		RTSALLOC_ASSERT(fragment_size <= FRAGMENT_SIZE_MAX);
		RTSALLOC_ASSERT(fragment_size >= FRAGMENT_SIZE_MIN);
		RTSALLOC_ASSERT(fragment_size >= amount + RTSALLOC_ALIGNMENT);
		RTSALLOC_ASSERT((fragment_size & (fragment_size - 1U)) == 0U);

		const uint_fast8_t optimal_bin_index = log2Ceil(fragment_size / FRAGMENT_SIZE_MIN); 
		RTSALLOC_ASSERT(optimal_bin_index < NUM_BINS_MAX);
		const size_t candidate_bin_mask = ~(pow2(optimal_bin_index) - 1U);

		const size_t suitable_bins = handle->nonempty_bin_mask & candidate_bin_mask;
		const size_t smallest_bin_mask = suitable_bins & ~(suitable_bins - 1U);
		if (RTSALLOC_LIKELY(smallest_bin_mask != 0))
		{
			RTSALLOC_ASSERT((smallest_bin_mask & (smallest_bin_mask - 1U)) == 0U); 
			const uint_fast8_t bin_index = log2Floor(smallest_bin_mask);
			RTSALLOC_ASSERT(bin_index >= optimal_bin_index);
			RTSALLOC_ASSERT(bin_index < NUM_BINS_MAX);

			Fragment* const frag = handle->bins[bin_index];
			RTSALLOC_ASSERT(frag != NULL);
			RTSALLOC_ASSERT(frag->header.size >= fragment_size);
			RTSALLOC_ASSERT((frag->header.size % FRAGMENT_SIZE_MIN) == 0U);
			RTSALLOC_ASSERT(!frag->header.used);
			unbin(handle, frag);

			const size_t leftover = frag->header.size - fragment_size;
			frag->header.size = fragment_size;
			RTSALLOC_ASSERT(leftover < handle->diagnostics.capacity); 
			RTSALLOC_ASSERT(leftover % FRAGMENT_SIZE_MIN == 0U);   
			if (RTSALLOC_LIKELY(leftover >= FRAGMENT_SIZE_MIN))
			{
				Fragment* const new_frag = (Fragment*)(void*)(((char*)frag) + fragment_size);
				RTSALLOC_ASSERT(((size_t)new_frag) % RTSALLOC_ALIGNMENT == 0U);
				new_frag->header.size = leftover;
				new_frag->header.used = false;
				interlink(new_frag, frag->header.next);
				interlink(frag, new_frag);
				rebin(handle, new_frag);
			}

			RTSALLOC_ASSERT((handle->diagnostics.allocated % FRAGMENT_SIZE_MIN) == 0U);
			handle->diagnostics.allocated += fragment_size;
			RTSALLOC_ASSERT(handle->diagnostics.allocated <= handle->diagnostics.capacity);
			if (RTSALLOC_LIKELY(handle->diagnostics.peak_allocated < handle->diagnostics.allocated))
			{
				handle->diagnostics.peak_allocated = handle->diagnostics.allocated;
			}

			RTSALLOC_ASSERT(frag->header.size >= amount + RTSALLOC_ALIGNMENT);
			frag->header.used = true;

			out = ((char*)frag) + RTSALLOC_ALIGNMENT;
		}
	}

	if (RTSALLOC_LIKELY(handle->diagnostics.peak_request_size < amount))
	{
		handle->diagnostics.peak_request_size = amount;
	}
	if (RTSALLOC_LIKELY((out == NULL) && (amount > 0U)))
	{
		handle->diagnostics.oom_count++;
	}

	return out;
}

void RTSAllocFree(RTSAllocInstance* const handle, void* const pointer)
{
	RTSALLOC_ASSERT(handle != NULL);
	RTSALLOC_ASSERT(handle->diagnostics.capacity <= FRAGMENT_SIZE_MAX);
	if (RTSALLOC_LIKELY(pointer != NULL)) 
	{
		Fragment* const frag = (Fragment*)(void*)(((char*)pointer) - RTSALLOC_ALIGNMENT);

		RTSALLOC_ASSERT(((size_t)frag) % sizeof(Fragment*) == 0U);
		RTSALLOC_ASSERT(((size_t)frag) >= (((size_t)handle) + INSTANCE_SIZE_PADDED));
		RTSALLOC_ASSERT(((size_t)frag) <=
			(((size_t)handle) + INSTANCE_SIZE_PADDED + handle->diagnostics.capacity - FRAGMENT_SIZE_MIN));
		RTSALLOC_ASSERT(frag->header.used);
		RTSALLOC_ASSERT(((size_t)frag->header.next) % sizeof(Fragment*) == 0U);
		RTSALLOC_ASSERT(((size_t)frag->header.prev) % sizeof(Fragment*) == 0U);
		RTSALLOC_ASSERT(frag->header.size >= FRAGMENT_SIZE_MIN);
		RTSALLOC_ASSERT(frag->header.size <= handle->diagnostics.capacity);
		RTSALLOC_ASSERT((frag->header.size % FRAGMENT_SIZE_MIN) == 0U);

		frag->header.used = false;

		RTSALLOC_ASSERT(handle->diagnostics.allocated >= frag->header.size);
		handle->diagnostics.allocated -= frag->header.size;

		Fragment* const prev = frag->header.prev;
		Fragment* const next = frag->header.next;
		const bool      join_left = (prev != NULL) && (!prev->header.used);
		const bool      join_right = (next != NULL) && (!next->header.used);
		if (join_left && join_right)
		{
			unbin(handle, prev);
			unbin(handle, next);
			prev->header.size += frag->header.size + next->header.size;
			frag->header.size = 0; 
			next->header.size = 0;
			RTSALLOC_ASSERT((prev->header.size % FRAGMENT_SIZE_MIN) == 0U);
			interlink(prev, next->header.next);
			rebin(handle, prev);
		}
		else if (join_left)
		{
			unbin(handle, prev);
			prev->header.size += frag->header.size;
			frag->header.size = 0;
			RTSALLOC_ASSERT((prev->header.size % FRAGMENT_SIZE_MIN) == 0U);
			interlink(prev, next);
			rebin(handle, prev);
		}
		else if (join_right)
		{
			unbin(handle, next);
			frag->header.size += next->header.size;
			next->header.size = 0;
			RTSALLOC_ASSERT((frag->header.size % FRAGMENT_SIZE_MIN) == 0U);
			interlink(frag, next->header.next);
			rebin(handle, frag);
		}
		else
		{
			rebin(handle, frag);
		}
	}
}

bool RTSAllocCheckHeapCorruption(const RTSAllocInstance* const handle)
{
	RTSALLOC_ASSERT(handle != NULL);
	bool valid = true;

	for (size_t i = 0; i < NUM_BINS_MAX; i++)
	{
		const bool mask_bit_set = (handle->nonempty_bin_mask & pow2((uint_fast8_t)i)) != 0U;
		const bool bin_nonempty = handle->bins[i] != NULL;
		valid = valid && (mask_bit_set == bin_nonempty);
	}

	const RTSAllocDiagnostics diag = handle->diagnostics;

	// Capacity check.
	valid = valid && (diag.capacity <= FRAGMENT_SIZE_MAX) && (diag.capacity >= FRAGMENT_SIZE_MIN) &&
		((diag.capacity % FRAGMENT_SIZE_MIN) == 0U);

	// Allocation info check.
	valid = valid && (diag.allocated <= diag.capacity) && ((diag.allocated % FRAGMENT_SIZE_MIN) == 0U) &&
		(diag.peak_allocated <= diag.capacity) && (diag.peak_allocated >= diag.allocated) &&
		((diag.peak_allocated % FRAGMENT_SIZE_MIN) == 0U);

	// Peak request check
	valid = valid && ((diag.peak_request_size < diag.capacity) || (diag.oom_count > 0U));
	if (diag.peak_request_size == 0U)
	{
		valid = valid && (diag.peak_allocated == 0U) && (diag.allocated == 0U) && (diag.oom_count == 0U);
	}
	else
	{
		valid = valid &&
			(((diag.peak_request_size + RTSALLOC_ALIGNMENT) <= diag.peak_allocated) || (diag.oom_count > 0U));
	}

	return valid;
}

RTSAllocDiagnostics RTSAllocGetDiagnostics(const RTSAllocInstance* const handle)
{
	RTSALLOC_ASSERT(handle != NULL);
	const RTSAllocDiagnostics out = handle->diagnostics;
	return out;
}