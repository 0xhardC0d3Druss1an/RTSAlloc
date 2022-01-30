# RTSAlloc
Аллокатор памяти для систем жесткого реального времени.

Аллокатор работает за константное время (всегда для наихудшего случая) и для наихудшего случая фрагментации памяти.

Основной аллокатор разработан на языке C99/C11 (файлы `rts_allocator.h` и `rts_allocator.c`) по стандарту MISRA (в некоторых местах с допущениями).

Данный аллокатор полностью совместим с любыми архитектурами

Также есть пример реализации аллокатора для работы с типами/контейнерами, использующие `std::allocator_traits`.

## Использование
```cpp
void example_C()
{
	const size_t size = 1024;
	char[size] memory;
	RTSAllocInstance* inst = RTSAllocInit(memory, size);

	void* block = RTSAllocAllocate(inst, 64);
	// operate on memory
	RTSAllocFree(block);
}

void example_cpp()
{
	RTSAllocator<int> alloc_int;
	std::vector<int, decltype(alloc_int)> vec;

	vec.reserve(1'000);
	for (int i = 0; i < 1'000; ++i)
	{
		vec.push_back(i);
	}
	std::cout << vec.size() << "\n"; // 1000
	vec.clear();
	vec.shrink_to_fit();
}
```
