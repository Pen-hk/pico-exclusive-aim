cmake_minimum_required(VERSION 3.13)
project(pico_exclusive_aim)
include(pico_sdk_import.cmake)
pico_sdk_init()

add_executable(pico_exclusive_aim main.c)

target_link_libraries(pico_exclusive_aim
    pico_stdlib
    tinyusb_board
    tinyusb_device
    pico_usb_device
)

# 只启用HID，不启用CDC
pico_enable_stdio_usb(pico_exclusive_aim 0)
pico_enable_stdio_uart(pico_exclusive_aim 0)

# 设置USB描述符 - 伪装成普通鼠标
pico_set_usb_vid(pico_exclusive_aim 0x2E8A)
pico_set_usb_pid(pico_exclusive_aim 0x000A)
pico_set_usb_manufacturer(pico_exclusive_aim "Raspberry Pi")
pico_set_usb_product(pico_exclusive_aim "RPi Pico HID")

pico_add_extra_outputs(pico_exclusive_aim)