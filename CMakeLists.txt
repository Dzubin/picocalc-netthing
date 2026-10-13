# PicoCalc NetThing  (by Thomas Dzubin)
#
# Reuses the drivers from ./picocalc-text-starter-main (lcd, southbridge),
# which is picocalc-text-starter by Blair Leduc, plus the Raspberry Pi Pico SDK.
# Build it with the Raspberry Pi Pico VS Code extension, or from a terminal that
# has PICO_SDK_PATH set (see README.md).

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in the Pico SDK (must be before project())
include(pico_sdk_import.cmake)

project(picocalc-netthing C CXX ASM)

pico_sdk_init()

# This program needs the Wi-Fi chip, so only a Pico W or Pico 2 W board will do (-DPICO_BOARD=pico2_w or -DPICO_BOARD=pico_w)
if(NOT PICO_CYW43_SUPPORTED)
    message(FATAL_ERROR "netthing needs a board with the CYW43 Wi-Fi chip: configure with -DPICO_BOARD=pico2_w (or pico_w)")
endif()

# Location of the picocalc-text-starter drivers we build against
set(STARTER ${CMAKE_CURRENT_LIST_DIR}/picocalc-text-starter-main)

add_executable(picocalc-netthing
    netthing.c
    platform_pico.c
    ui.c
    link.c
    tools.c
    fetch.c
    certs.c
    pinglive.c
    trace.c
    ports.c
    survey.c
    menu.c
    lan.c
    gfx.c
    bt_pico.c
    btmenu.c
    btscan.c
    tether.c
    btfind.c
    btinspect.c
    bttrack.c
    btcrowd.c
    btdecode.c
    health.c
    meter.c
    dnscheck.c
    dnsquery.c
    srvtools.c
    usbnet.c
    usbnet_desc.c
    ${PICO_TINYUSB_PATH}/lib/networking/rndis_reports.c
    lanscan.c
    portlist.c
    pemselect.c
    url.c
    httpresp.c
    net_pico.c
    net_stream_pico.c
    settings.c
    timeutil.c
    wifi_pico.c
    storage_pico.c
    secret.c
    ${STARTER}/drivers/fat32.c
    ${STARTER}/drivers/sdcard.c
    ${STARTER}/drivers/lcd.c
    ${STARTER}/drivers/southbridge.c
    ${STARTER}/drivers/font-8x10.c
    ${STARTER}/drivers/font-5x10.c
)

if(COMMAND suppress_btstack_warnings)
    suppress_btstack_warnings()
endif()

pico_set_program_name(picocalc-netthing "picocalc-netthing")

# The program version comes from the VERSION line in netthing_const.h, so it is
# written down in one place only (a change there re-runs CMake).
file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/netthing_const.h VERSION_LINE
     REGEX "^#define VERSION ")
string(REGEX REPLACE "^#define VERSION \"V?([^\"]*)\".*" "\\1" PROGRAM_VERSION "${VERSION_LINE}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/netthing_const.h)
pico_set_program_version(picocalc-netthing "${PROGRAM_VERSION}")

# No serial console needed; keep USB free so ~ -> BOOTSEL is clean
pico_enable_stdio_uart(picocalc-netthing 0)
pico_enable_stdio_usb(picocalc-netthing 0)

target_compile_options(picocalc-netthing PRIVATE -Wall)

# The TLS layer must refuse a server whose certificate does not check out
# (the stack is 4 KB, the most the scratch RAM holds, because the TLS handshake runs on it)
target_compile_definitions(picocalc-netthing PRIVATE ALTCP_MBEDTLS_AUTHMODE=MBEDTLS_SSL_VERIFY_REQUIRED
    PICO_STACK_SIZE=0x1000)

# The SDK's altcp_tls_mbedtls.c reads fields that Mbed TLS 3 marks private
set_source_files_properties(${PICO_SDK_PATH}/src/rp2_common/pico_lwip/altcp_tls_mbedtls.c
    PROPERTIES COMPILE_DEFINITIONS MBEDTLS_ALLOW_PRIVATE_ACCESS)

target_include_directories(picocalc-netthing PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${STARTER}/drivers
    ${PICO_TINYUSB_PATH}/lib/networking
)

target_link_libraries(picocalc-netthing
    pico_stdlib
    pico_multicore
    hardware_gpio
    hardware_i2c
    hardware_spi
    hardware_pio
    hardware_clocks
    hardware_watchdog
    pico_cyw43_arch_lwip_threadsafe_background
    pico_btstack_ble
    pico_btstack_classic
    pico_btstack_cyw43
    tinyusb_device
    pico_unique_id
    pico_rand
    pico_lwip_mbedtls
    pico_mbedtls
)

# Bluetooth tethering is left out of the Pico W build (chip_limits.h): too little RAM
if(NOT PICO_PLATFORM STREQUAL "rp2040")
    target_sources(picocalc-netthing PRIVATE btpan.c btpanui.c)
    target_link_libraries(picocalc-netthing pico_btstack_bnep_lwip)
endif()

# The chip is part of every output file name (netthing-RP2350.uf2, and the matching
# .elf/.bin/.hex), so a downloaded file always says which chip it was built for.
if(PICO_PLATFORM MATCHES "^rp2350")
    set(CHIP_NAME RP2350)
elseif(PICO_PLATFORM STREQUAL "rp2040")
    set(CHIP_NAME RP2040)
else()
    message(FATAL_ERROR "netthing is for the Pico 2 W (RP2350) or the Pico W (RP2040)")
endif()
set_target_properties(picocalc-netthing PROPERTIES OUTPUT_NAME netthing-${CHIP_NAME})

pico_add_extra_outputs(picocalc-netthing)

# Copy the finished .uf2 (same name) to the top-level project folder.
add_custom_command(TARGET picocalc-netthing POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE_DIR:picocalc-netthing>/$<TARGET_FILE_BASE_NAME:picocalc-netthing>.uf2
            ${CMAKE_CURRENT_LIST_DIR}/
)
