#ifndef RTSALLOCATORINSTANCE_HPP
#define RTSALLOCATORINSTANCE_HPP

#include "settings.hpp"
#include "Singleton.hpp"
#include "rts/rts_allocator.h"

class RTSAllocatorInstance : public Singleton<RTSAllocatorInstance>
{
	size_t mem_amount = 1_mb;
	void* memory = nullptr;
	RTSAllocInstance* inst = nullptr;

public:
	RTSAllocatorInstance()
	{
		memory = ALLIGNED_MALLOC(mem_amount, RTSALLOC_ALIGNMENT);
		inst = RTSAllocInit(memory, mem_amount);
	}

	~RTSAllocatorInstance()
	{
		if (memory)
			ALLIGNED_FREE(memory);
	}

	struct details
	{
		size_t sz;
		void* mem;
		RTSAllocInstance* inst;
	};

	details Details()
	{
		return { mem_amount, memory, inst };
	}

	void* Allocate(size_t sz)
	{
		return RTSAllocAllocate(inst, sz);
	}

	void Free(void* ptr)
	{
		RTSAllocFree(inst, ptr);
	}
};

#endif // !RTSALLOCATORINSTANCE_HPP
