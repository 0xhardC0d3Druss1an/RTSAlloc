#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#define _ISOC11_SOURCE
#include <cstdlib>

#if _WIN32
	#define ALLIGNED_MALLOC(size, alignment) _aligned_malloc(size, alignment)
	#define ALLIGNED_FREE(ptr) _aligned_free(ptr)
#else
	#define ALLIGNED_MALLOC(size, alignment) std::aligned_alloc(alignment, size)
	#define ALLIGNED_FREE(ptr) std::free(ptr)
#endif

constexpr auto operator ""_kb(unsigned long long sz) {
	return 1024 * sz;
}

constexpr auto operator ""_mb(unsigned long long sz) {
	return 1024 * 1024 * sz;
}

constexpr auto operator ""_gb(unsigned long long sz) {
	return 1024 * 1024 * 1024 * sz;
}

#endif // !SETTINGS_HPP
