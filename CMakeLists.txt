cmake_minimum_required(VERSION 3.30)
project(falcon)

set(CMAKE_CXX_STANDARD 20)

if(WIN32)
    set(FALCON_BACKEND src/falcon_windows.cpp)
    set(WINDOWS_LIBS ws2_32)
else ()
    set(FALCON_BACKEND src/falcon_posix.cpp)
endif (WIN32)

add_library(falcon STATIC inc/falcon.h src/falcon_common.cpp ${FALCON_BACKEND} inc/Stream.h src/Stream.cpp)
target_include_directories(falcon PUBLIC inc)
target_link_libraries(falcon PUBLIC spdlog::spdlog_header_only fmt::fmt-header-only)

add_subdirectory(externals)
add_subdirectory(samples)
add_subdirectory(tests)

target_link_libraries(falcon PRIVATE ${WINDOWS_LIBS})
target_link_libraries(client PRIVATE ${WINDOWS_LIBS})
target_link_libraries(server PRIVATE ${WINDOWS_LIBS})