cmake_minimum_required(VERSION 3.31)
project(voice_chat)

set(CMAKE_CXX_STANDARD 20)

#Apple Silicon
include_directories(/opt/homebrew/opt/portaudio/include)
link_directories(/opt/homebrew/opt/portaudio/lib)

# Настройка для Windows
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    set(PLATFORM_LIBS ws2_32 winmm)
else()
    # Настройка для macOS/Linux
    set(PLATFORM_LIBS pthread)
endif()

# Сервер
add_executable(voice_server server.cpp)
target_link_libraries(voice_server portaudio ${PLATFORM_LIBS})

# Клиент
add_executable(voice_client client.cpp)
target_link_libraries(voice_client portaudio ${PLATFORM_LIBS})
