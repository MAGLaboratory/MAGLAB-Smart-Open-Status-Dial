cmake_minimum_required(VERSION 3.20)

project(m5dial_host_sim LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../apps/m5dial-timer/components/lvgl/lv_conf.h" CACHE STRING "" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../apps/m5dial-timer/components/lvgl
                 ${CMAKE_CURRENT_BINARY_DIR}/lvgl)

find_package(SDL2 REQUIRED)

add_executable(m5dial_host_sim
    src/sdl_driver.cpp
    src/sim_main.cpp
    ../../apps/m5dial-timer/components/ui/src/ui_root.cpp
)

target_include_directories(m5dial_host_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ../../apps/m5dial-timer/components/ui/include
    ../../apps/m5dial-timer/components/timer/include
    ../../apps/m5dial-timer/components/lvgl
)

target_link_libraries(m5dial_host_sim PRIVATE
    lvgl
    SDL2::SDL2
    SDL2::SDL2main
)

if (MSVC)
    target_compile_definitions(m5dial_host_sim PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
