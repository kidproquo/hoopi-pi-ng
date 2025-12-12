cmake_minimum_required(VERSION 3.15)

# Find libsndfile for WAV file loading and libmpg123 for MP3 file loading using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED sndfile)
pkg_check_modules(MPG123 REQUIRED libmpg123)

# HoopiPi standalone application (JACK version)
add_executable(HoopiPi
    main_jack.cpp
    BackingTrack.cpp
)

target_include_directories(HoopiPi PRIVATE ${SNDFILE_INCLUDE_DIRS} ${MPG123_INCLUDE_DIRS})

target_link_libraries(HoopiPi
    PRIVATE
        HoopiPiEngine
        ${SNDFILE_LIBRARIES}
        ${MPG123_LIBRARIES}
)

# Install
install(TARGETS HoopiPi
    RUNTIME DESTINATION bin
)

# Install config template
install(FILES config.ini
    DESTINATION share/HoopiPi
)
