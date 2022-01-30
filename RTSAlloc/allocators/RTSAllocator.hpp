#ifndef RTSALLOCATOR_HPP
#define RTSALLOCATOR_HPP

#include "RTSAllocatorInstance.hpp"

template <class T>
struct RTSAllocator
{
	typedef T value_type;

	RTSAllocator() = default;
	template <class U> constexpr RTSAllocator(const RTSAllocator <U>&) noexcept {}

	[[nodiscard]] T* allocate(std::size_t n)
	{
		auto sz = n * sizeof(T);
		auto& inst = RTSAllocatorInstance::Get();
		return (T*)inst.Allocate(sz);
	}

	void deallocate(T* p, std::size_t n) noexcept
	{
		auto& inst = RTSAllocatorInstance::Get();
		inst.Free(p);
	}

private:
	void report(T* p, std::size_t n, bool alloc = true) const 
	{
		std::cout << (alloc ? "Allocated: " : "Deallocated: ") << sizeof(T) * n
			<< " bytes at " << std::hex << std::showbase
			<< reinterpret_cast<void*>(p) << std::dec << "\n";
	}
};

template <class T, class U>
bool operator==(const RTSAllocator <T>&, const RTSAllocator <U>&) { return true; }
template <class T, class U>
bool operator!=(const RTSAllocator <T>&, const RTSAllocator <U>&) { return false; }


#endif // !RTSALLOCATOR_HPP
