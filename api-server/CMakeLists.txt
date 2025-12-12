cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch cpp-httplib
include(FetchContent)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
)

FetchContent_MakeAvailable(httplib)

# JSON library is already available via RTNeural/NeuralAudio dependencies
# We'll use the same nlohmann/json that NeuralAudio uses

# API Server executable
add_executable(HoopiPiAPI
    main.cpp
)

target_include_directories(HoopiPiAPI
    PRIVATE
        ${CMAKE_SOURCE_DIR}/dependencies/NeuralAudio/deps/NeuralAmpModelerCore/Dependencies
)

target_link_libraries(HoopiPiAPI
    PRIVATE
        httplib::httplib
)

# Install
install(TARGETS HoopiPiAPI
    RUNTIME DESTINATION bin
)
