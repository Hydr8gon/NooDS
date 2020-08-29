cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast -flto")

add_library(noods-core SHARED
            cpp/interface.cpp
            ../common/screen_layout.cpp
            ../cartridge.cpp
            ../core.cpp
            ../cp15.cpp
            ../dma.cpp
            ../gpu.cpp
            ../gpu_2d.cpp
            ../gpu_3d.cpp
            ../gpu_3d_renderer.cpp
            ../input.cpp
            ../interpreter.cpp
            ../ipc.cpp
            ../math.cpp
            ../memory.cpp
            ../rtc.cpp
            ../settings.cpp
            ../spi.cpp
            ../spu.cpp
            ../timers.cpp
            ../wifi.cpp)

target_link_libraries(noods-core jnigraphics)
