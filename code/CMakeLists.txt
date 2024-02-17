cmake_minimum_required(VERSION 3.12)

set(CMAKE_C_STANDARD 11)

set(PICO_PLATFORM rp2040)
set(BOARD pico_sdk)

include(pico_sdk_import.cmake)

# Run from SRAM
set(PICO_DEFAULT_BINARY_TYPE copy_to_ram)

set(PROJECT CPAK2040)

project(${PROJECT})

pico_sdk_init()

add_compile_options( -Ofast -Wall )

add_executable(${PROJECT})

pico_enable_stdio_usb(${PROJECT} 0)
pico_enable_stdio_uart(${PROJECT} 1)        

pico_add_extra_outputs(${PROJECT})

target_sources(${PROJECT} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/main.c
                                 ${CMAKE_CURRENT_SOURCE_DIR}/usb_descriptors.c
                                 ${CMAKE_CURRENT_SOURCE_DIR}/msc_disk.c)
                                 
# Make sure TinyUSB can find tusb_config.h
target_include_directories(${PROJECT} PUBLIC
                           ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(${PROJECT} pico_stdlib pico_multicore hardware_flash tinyusb_device tinyusb_board)
