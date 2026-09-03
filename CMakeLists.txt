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

# 伪装USB描述符
# VID/PID 伪装成微软鼠标，系统看到的就是一个普通微软鼠标
# 而不是 Raspberry Pi Pico，Vanguard 扫不到任何可疑硬件
target_compile_definitions(pico_exclusive_aim PRIVATE
    USBD_VID=0x045E
    USBD_PID=0x0040
    BOARD_DEVICE_PRODUCT_STRING="Microsoft USB Mouse"
    BOARD_DEVICE_MANUFACTURER_STRING="Microsoft"
)

pico_add_extra_outputs(pico_exclusive_aim)