cmake_minimum_required(VERSION 3.13)
project(pico_exclusive_aim)
include(pico_sdk_import.cmake)
pico_sdk_init()

add_executable(pico_exclusive_aim main.c)

target_link_libraries(pico_exclusive_aim
    pico_stdlib
    tinyusb_board
    tinyusb_device
)

# 只启用HID，不启用CDC
pico_enable_stdio_usb(pico_exclusive_aim 0)
pico_enable_stdio_uart(pico_exclusive_aim 0)

pico_add_extra_outputs(pico_exclusive_aim)
