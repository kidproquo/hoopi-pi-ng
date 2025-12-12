cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find ALSA and JACK
find_package(ALSA REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JACK REQUIRED jack)

# HoopiPi Engine library
add_library(HoopiPiEngine
    Engine.cpp
    Engine.h
    ModelLoader.cpp
    ModelLoader.h
    NoiseGate.cpp
    NoiseGate.h
    DCBlocker.cpp
    DCBlocker.h
    Reverb.cpp
    Reverb.h
    AlsaBackend.cpp
    AlsaBackend.h
    JackBackend.cpp
    JackBackend.h
    IPCServer.cpp
    IPCServer.h
)

# Enable position-independent code for shared library compatibility
set_target_properties(HoopiPiEngine PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# Propagate BUILD_STATIC_RTNEURAL flag from NeuralAudio
if(BUILD_STATIC_RTNEURAL)
    target_compile_definitions(HoopiPiEngine PRIVATE BUILD_STATIC_RTNEURAL)
endif()

target_include_directories(HoopiPiEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/dependencies/NeuralAudio/NeuralAudio
        ${CMAKE_SOURCE_DIR}/dependencies
    PRIVATE
        ${ALSA_INCLUDE_DIRS}
        ${JACK_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/dependencies/NeuralAudio/deps/NeuralAmpModelerCore/Dependencies
)

target_link_libraries(HoopiPiEngine
    PUBLIC
        NeuralAudio
    PRIVATE
        ${ALSA_LIBRARIES}
        ${JACK_LIBRARIES}
        pthread
)

# HoopiPi C API library
add_library(HoopiPiCAPI SHARED
    HoopiPiCApi.cpp
    HoopiPiCApi.h
)

target_include_directories(HoopiPiCAPI
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(HoopiPiCAPI
    PRIVATE
        HoopiPiEngine
)

# Install targets
install(TARGETS HoopiPiEngine HoopiPiCAPI
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    HoopiPiCApi.h
    Engine.h
    ModelLoader.h
    AlsaBackend.h
    NoiseGate.h
    DCBlocker.h
    DESTINATION include/HoopiPi
)
