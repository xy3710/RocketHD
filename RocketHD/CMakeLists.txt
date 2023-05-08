cmake_minimum_required(VERSION 3.16.3)
project(RocketHD)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build and include wifibroadcast
include(lib/wifibroadcast/WBLib.cmake)

add_library(RocketLib STATIC) # initialized below
target_include_directories(RocketLib PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(RocketLib PUBLIC ${WB_TARGET_LINK_LIBRARIES})

set(sources
    "src/gst_appsink_helper.hpp"
    "src/gstreamerstream.cpp"
    "src/rtp_eof_helper.cpp"
    "src/UdpBlockedWBTransmitter.hpp"
    "src/wfb_tx.cpp"
    "src/wb_link.cpp"
    "src/wifi_command_helper.cpp"
    "src/rocket.cpp"
    "include/gstreamerstream.hpp"
    "include/rtp_eof_helper.hpp"
    "include/gstreamerstream.hpp"
    "include/rtp_eof_helper.hpp"
    )
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${sources})
target_sources(RocketLib PRIVATE ${sources})

# GSTREAMER
find_package(PkgConfig REQUIRED)
pkg_search_module(GST REQUIRED gstreamer-1.0>=1.4
        gstreamer-sdp-1.0>=1.4
        gstreamer-video-1.0>=1.4
        gstreamer-app-1.0>=1.4)
## xx
# https://stackoverflow.com/questions/64078308/how-to-write-a-cmake-file-which-successfully-links-gstreamer-basic-tutorial-5
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-sdp REQUIRED IMPORTED_TARGET gstreamer-sdp-1.0>=1.4)
pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)
target_link_libraries(RocketLib PRIVATE
        PkgConfig::gstreamer
        PkgConfig::gstreamer-sdp
        PkgConfig::gstreamer-app
        PkgConfig::gstreamer-video)
target_include_directories(RocketLib PUBLIC ${GST_INCLUDE_DIRS})

add_executable(wfb_tx src/wfb_tx.cpp)
target_link_libraries(wfb_tx RocketLib)

add_executable(rocket src/rocket.cpp)
target_link_libraries(rocket RocketLib)