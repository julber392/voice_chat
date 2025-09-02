cmake_minimum_required(VERSION 3.31)
project(voice_chat)

set(CMAKE_CXX_STANDARD 20)
find_package(PortAudio QUIET)
if(NOT PortAudio_FOUND)
    message(STATUS "PortAudio not found via find_package, trying pkg-config")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PORTAUDIO portaudio-2.0)
    if(PORTAUDIO_FOUND)
        add_library(PortAudio::PortAudio UNKNOWN IMPORTED)
        if(APPLE)
            set_target_properties(PortAudio::PortAudio PROPERTIES
                    IMPORTED_LOCATION "${PORTAUDIO_LIBDIR}/libportaudio.dylib"
                    INTERFACE_INCLUDE_DIRECTORIES "${PORTAUDIO_INCLUDEDIR}"
            )
        else()
            set_target_properties(PortAudio::PortAudio PROPERTIES
                    IMPORTED_LOCATION "${PORTAUDIO_LIBDIR}/libportaudio.so"
                    INTERFACE_INCLUDE_DIRECTORIES "${PORTAUDIO_INCLUDEDIR}"
            )
        endif()
    else()
        message(FATAL_ERROR "PortAudio not found. Install it (e.g., apt install libportaudio2 libportaudio-dev) and re-run CMake.")
    endif()
endif()
add_executable(voice_chat main.cpp)


