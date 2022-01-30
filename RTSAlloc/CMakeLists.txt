cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
set(PROJECT_SOURCES "main.cpp" "settings.hpp")
list(APPEND PROJECT_SOURCES "rts/rts_allocator.c" "rts/rts_allocator.h")
list(APPEND PROJECT_SOURCES "allocators/Singleton.hpp" "allocators/RTSAllocatorInstance.hpp")
list(APPEND PROJECT_SOURCES "allocators/RTSAllocator.hpp")

message(${PROJECT_SOURCES})
add_executable (RTSAlloc ${PROJECT_SOURCES})
