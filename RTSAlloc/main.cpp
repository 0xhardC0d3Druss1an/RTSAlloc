#include <vector>
#include <string>
#include <iostream>
#include "allocators/RTSAllocator.hpp"

int main()
{
	RTSAllocator<char> alloc;
	using str8 = std::basic_string<char, std::char_traits<char>, RTSAllocator<char>>;
	
	str8 str(alloc);
	str = "Some";
	std::cout << str.capacity() << "\n";

	str = "Some string hello world big data";
	std::cout << str.capacity() << "\n";

	str.clear();
	str.shrink_to_fit();
	std::cout << str.capacity() << "\n";

	RTSAllocator<int> alloc_int;
	std::vector<int, decltype(alloc_int)> vec;
	
	vec.reserve(1'000);
	for (int i = 0; i < 1'000; ++i)
	{
		vec.push_back(i);
	}
	std::cout << vec.size() << "\n";

	return 0;
}