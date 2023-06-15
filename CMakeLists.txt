cmake_minimum_required(VERSION 3.0.0)
project(opencvtest VERSION 0.1.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)

include(CTest)
enable_testing()

find_package( OpenCV REQUIRED )
find_package( amqpcpp REQUIRED )
find_package( Boost REQUIRED )

include_directories( 
    ${OpenCV_INCLUDE_DIRS} 
    ${Boost_INCLUDE_DIRS} 
    ${amqpcpp_INCLUDE_DIRS} )

include_directories(inc/)
link_directories(src/)

add_library(message_bus_client STATIC src/message_bus_client.cpp)
add_library(inference STATIC src/inference.cpp)
add_library(yolo_v8 STATIC src/yolo_v8.cpp)
add_library(yolo_v5 STATIC src/yolo_v5.cpp)

add_executable(opencvtest main.cpp)

add_dependencies(opencvtest amqpcpp)

target_link_libraries( 
    opencvtest
    amqpcpp 
    boost_system 
    pthread 
    dl 
    ssl
    inference
    message_bus_client
    yolo_v5
    yolo_v8
    ${OpenCV_LIBS} )

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
