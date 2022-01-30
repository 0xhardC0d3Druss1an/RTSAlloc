#ifndef RTS_ALLOCATOR_H
#define RTS_ALLOCATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTSALLOC_ALIGNMENT (sizeof(void*) * 4U)

	// encapsulate
	typedef struct RTSAllocInstance RTSAllocInstance;

	typedef struct
	{
		size_t capacity;
		size_t allocated;
		size_t peak_allocated;
		size_t peak_request_size;
		uint64_t oom_count;
	} RTSAllocDiagnostics;

	RTSAllocInstance* RTSAllocInit(void* const base, const size_t size);
	void* RTSAllocAllocate(RTSAllocInstance* const handle, const size_t amount);
	void RTSAllocFree(RTSAllocInstance* const handle, void* const pointer);
	bool RTSAllocCheckHeapCorruption(const RTSAllocInstance* const handle);
	RTSAllocDiagnostics RTSAllocGetDiagnostics(const RTSAllocInstance* const handle);

#ifdef __cplusplus
}
#endif

#endif // !RTS_ALLOCATOR_H
