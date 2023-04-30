cmake_minimum_required(VERSION 3.16.3)
project(RocketHD)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build and include wifibroadcast
include(lib/wifibroadcast/WBLib.cmake)

add_library(RocketLib STATIC) # initialized below
target_include_directories(RocketLib PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(RocketLib PUBLIC ${WB_TARGET_LINK_LIBRARIES})
target_sources(RocketLib PRIVATE src/rtp_eof_helper.cpp)

add_executable(wfb_tx src/wfb_tx.cpp)
target_link_libraries(wfb_tx RocketLib)