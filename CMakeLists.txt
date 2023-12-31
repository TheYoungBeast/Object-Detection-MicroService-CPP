cmake_minimum_required(VERSION 3.0.0)
project(micro_od VERSION 0.1.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)

#add_compile_options(-fsanitize=undefined)
#add_link_options(-fsanitize=undefined)

set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Wall -Wextra -Wpedantic")

#set(CMAKE_BUILD_TYPE Debug)

include(CTest)
enable_testing()

find_package( OpenCV REQUIRED )
find_package( amqpcpp REQUIRED )
find_package( 
    Boost 1.80.0 REQUIRED 
    COMPONENTS  json program_options REQUIRED)
find_package(spdlog REQUIRED)

# Add pkg-config functionality
find_package(PkgConfig REQUIRED) 

# search for libevent.pc
pkg_search_module(EVENT REQUIRED libevent)

include_directories( 
    ${OpenCV_INCLUDE_DIRS} 
    ${Boost_INCLUDE_DIRS} 
    ${amqpcpp_INCLUDE_DIRS} )

include_directories(
    inc/
    ${EVENT_INCLUDE_DIRS})

file(GLOB_RECURSE SOURCES
    inc/*.h
    inc/*.hpp
    src/*.cpp
)

#target_link_libraries( micro_od 
link_libraries( 
    amqpcpp
    event
    pthread 
    dl
    ssl
    ${Boost_LIBRARIES}
    ${OpenCV_LIBS}
    ${EVENT_LIBRARIES}
    spdlog::spdlog_header_only )

add_executable(micro_od main.cpp ${SOURCES})

add_dependencies(micro_od amqpcpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)