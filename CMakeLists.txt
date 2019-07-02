cmake_minimum_required(VERSION 3.14)
project(nfc_st_srx C)

set(CMAKE_C_STANDARD 99)

include(FindPkgConfig)
if(NOT PKG_CONFIG_FOUND)
    message(FATAL_ERROR "pkg-config not found!" )
endif()

pkg_check_modules(libnfc REQUIRED IMPORTED_TARGET libnfc)

add_executable(nfc_st_srx nfc-utils.h nfc-utils.c main.c st-srx.h st-srx.c)
target_link_libraries(nfc_st_srx PkgConfig::libnfc)