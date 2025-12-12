cmake_minimum_required(VERSION 3.15)

project(HoopiPi VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Release build for performance
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose Release or Debug" FORCE)
endif()

# ref: https://forums.raspberrypi.com/viewtopic.php?t=288404
# ARM-specific optimizations for Raspberry Pi 
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=native -mtune=native -march=native")
    message(STATUS "Enabling ARM 64 optimizations")
endif()

# Enable static RTNeural for better runtime performance (increases build time)
option(BUILD_STATIC_RTNEURAL "Don't build optimized static RTNeural models" OFF)

# Build NeuralAudio library with HoopiPi options disabled
# (we're building HoopiPi at the root level instead)
set(BUILD_HOOPIPI OFF CACHE BOOL "Don't build HoopiPi from NeuralAudio" FORCE)
set(BUILD_UTILS OFF CACHE BOOL "Don't build NeuralAudio utils by default" FORCE)

# Add NeuralAudio dependency
add_subdirectory(dependencies/NeuralAudio)

# Add HoopiPi components
add_subdirectory(HoopiPi)
add_subdirectory(standalone)

# Add API server
add_subdirectory(api-server)
