cmake_minimum_required(VERSION 3.5.0)
project(mrpohos C CXX)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

set(UNICORN_TARGET_ARCH "aarch64" CACHE STRING "Unicorn target architecture")
set(UNICORN_ARCH "arm" CACHE STRING "Unicorn architectures to include")

add_subdirectory(unicorn)

set(VMRP_DIR ${NATIVERENDER_ROOT_PATH}/vmrp)

set(VMRP_SOURCES
    ${VMRP_DIR}/vmrp.c
    ${VMRP_DIR}/bridge.c
    ${VMRP_DIR}/memory.c
    ${VMRP_DIR}/fileLib.c
    ${VMRP_DIR}/network.c
    ${VMRP_DIR}/rbtree.c
    ${VMRP_DIR}/utils.c
    ${VMRP_DIR}/platform_harmony.c
)

include_directories(
    ${NATIVERENDER_ROOT_PATH}
    ${NATIVERENDER_ROOT_PATH}/include
    ${VMRP_DIR}
    ${VMRP_DIR}/header
    ${UNICORN_INC}
)

add_library(entry SHARED
    napi_init.cpp
    ${VMRP_SOURCES}
)

target_compile_definitions(entry PRIVATE
    __HARMONYOS__
    UNICORN_HAS_ARM
    VMRP
)

target_link_libraries(entry PUBLIC
    libace_napi.z.so
    unicorn
    libz.so
)
