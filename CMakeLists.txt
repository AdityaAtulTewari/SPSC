cmake_minimum_required(VERSION 3.10)
project(SPSC)

set(CMAKE_CXX_STANDARD 17)

find_package( Threads )

#set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (default Release)" FORCE)

add_executable(SPSC src/main.cpp src/timing.h src/spsc.h)

if(NOGEM5)
  set(CMAKE_CXX_FLAGS "-DNOGEM5=true ${CMAKE_CXX_FLAGS}")
#  target_link_libraries(Counter libvl.a)
#else()
#  target_include_directories(Counter PRIVATE "/benchmarks/VirtualLink/near-data-sim/include/")
#  target_link_libraries(Counter /benchmarks/VirtualLink/near-data-sim/util/m5/libm5.a)
#  target_include_directories(Counter PRIVATE "/benchmarks/VirtualLink/libvl")
#  target_link_libraries(Counter /benchmarks/VirtualLink/libvl/libvl/libvl.a)
#
endif()

target_link_libraries(SPSC ${CMAKE_THREAD_LIBS_INIT})
