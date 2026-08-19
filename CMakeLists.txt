cmake_minimum_required(VERSION 3.20)
project(YTDownloader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN)

set(SOURCES
    src/main.cpp
    src/download_manager.cpp
    src/youtube_parser.cpp
    src/http_server.cpp
)

set(HEADERS
    src/download_manager.h
    src/youtube_parser.h
    src/http_server.h
)

add_executable(YTDownloaderCore ${SOURCES} ${HEADERS})

if(WIN32)
    target_link_libraries(YTDownloaderCore PRIVATE ws2_32 shlwapi)
endif()

add_custom_command(
    TARGET YTDownloaderCore POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/ui $<TARGET_FILE_DIR:YTDownloaderCore>/ui
)

message(STATUS "YTDownloader Core CMake configuration complete.")
