cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -DLOG_LEVEL=0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast -flto")

add_library(noods-core SHARED
            cpp/interface.cpp
            ../nds_icon.cpp
            ../screen_layout.cpp
            ../../core/core.cpp
            ../../core/save_states.cpp
            ../../core/settings.cpp
            ../../core/arm/cp15.cpp
            ../../core/arm/interpreter.cpp
            ../../core/arm/interpreter_alu.cpp
            ../../core/arm/interpreter_branch.cpp
            ../../core/arm/interpreter_lookup.cpp
            ../../core/arm/interpreter_transfer.cpp
            ../../core/arm/timers.cpp
            ../../core/gpu/gpu.cpp
            ../../core/gpu/gpu_2d.cpp
            ../../core/gpu/gpu_3d.cpp
            ../../core/gpu/gpu_3d_renderer.cpp
            ../../core/hle/action_replay.cpp
            ../../core/hle/dldi.cpp
            ../../core/hle/hle_arm7.cpp
            ../../core/hle/hle_bios.cpp
            ../../core/io/aes.cpp
            ../../core/io/div_sqrt.cpp
            ../../core/io/input.cpp
            ../../core/io/ipc.cpp
            ../../core/io/rtc.cpp
            ../../core/io/spi.cpp
            ../../core/io/spu.cpp
            ../../core/io/wifi.cpp
            ../../core/memory/cartridge.cpp
            ../../core/memory/dma.cpp
            ../../core/memory/memory.cpp)

target_link_libraries(noods-core jnigraphics OpenSLES)
