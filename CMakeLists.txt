cmake_minimum_required(VERSION 3.0.0)
project(micro_od VERSION 0.1.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)

include(CTest)
enable_testing()

find_package( OpenCV REQUIRED )
find_package( amqpcpp REQUIRED )
find_package( 
    Boost 1.80.0 REQUIRED 
    COMPONENTS  json)
find_package(spdlog REQUIRED)

include_directories( 
    ${OpenCV_INCLUDE_DIRS} 
    ${Boost_INCLUDE_DIRS} 
    ${amqpcpp_INCLUDE_DIRS} )

include_directories(inc/)

file(GLOB_RECURSE SOURCES
    inc/*.h
    inc/*.hpp
    src/*.cpp
)

add_executable(micro_od main.cpp ${SOURCES})

add_dependencies(micro_od amqpcpp)

target_link_libraries( 
    micro_od
    amqpcpp 
    boost_system
    boost_json 
    pthread 
    dl 
    ssl
    ${OpenCV_LIBS}
    spdlog::spdlog_header_only )

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
