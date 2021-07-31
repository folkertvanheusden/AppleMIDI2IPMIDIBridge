project(a2im)
cmake_minimum_required(VERSION 3.2)
add_definitions("-std=gnu++17 -Wall -pedantic -Werror")

set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fstack-protector-strong")
set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer")

add_executable(
  a2im
  a2im.cpp
  mdns_rtpmidi.cpp
)

target_link_libraries(a2im fmt rtpmidid fmt)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)
target_link_libraries(a2im Threads::Threads)

include(FindPkgConfig)

pkg_check_modules(AVAHI REQUIRED avahi-client)
target_link_libraries(a2im ${AVAHI_LIBRARIES})
target_include_directories(a2im PUBLIC ${AVAHI_INCLUDE_DIRS})
target_compile_options(a2im PUBLIC ${AVAHI_CFLAGS_OTHER})

pkg_check_modules(FMT REQUIRED fmt)
target_link_libraries(a2im ${FMT_LIBRARIES})
target_include_directories(a2im PUBLIC ${FMT_INCLUDE_DIRS})
target_compile_options(a2im PUBLIC ${FMT_CFLAGS_OTHER})

set_target_properties(a2im PROPERTIES OUTPUT_NAME a2im)
