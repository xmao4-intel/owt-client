project(owt_client)
cmake_minimum_required(VERSION 2.8)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

include_directories(${CMAKE_SOURCE_DIR}/include)
link_directories(${CMAKE_SOURCE_DIR}/lib)

add_executable(owt_client
    owt_client.cpp
    owt_signalingchannel.cpp)

set_target_properties(owt_client PROPERTIES COMPILE_DEFINITIONS "WEBRTC_LINUX;WEBRTC_POSIX;USE_X11=1;")
target_link_libraries(owt_client
    ${SDL2_LIBRARIES}
    boost_system
    sioclient
    owt)
