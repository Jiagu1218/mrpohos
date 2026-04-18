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

option(VMRP_NATIVE_MIDI "Link FluidSynth+OHAudio for real-time MIDI playback" OFF)
set(VMRP_FLUIDSYNTH_ROOT "" CACHE PATH "FluidSynth 前缀：含 include/ 与 lib/（可为 thirdparty/fluidsynth/<ABI>）")

set(_VMRP_MIDI_LINK_NATIVE 0)
if(VMRP_NATIVE_MIDI)
    if(DEFINED OHOS_ARCH)
        set(_VMRP_OHOS_ARCH "${OHOS_ARCH}")
    else()
        set(_VMRP_OHOS_ARCH "arm64-v8a")
    endif()
    if(NOT VMRP_FLUIDSYNTH_ROOT)
        set(_VMRP_FLUID_BUNDLE "${NATIVERENDER_ROOT_PATH}/thirdparty/fluidsynth/${_VMRP_OHOS_ARCH}")
        if(EXISTS "${_VMRP_FLUID_BUNDLE}/include/fluidsynth.h")
            set(VMRP_FLUIDSYNTH_ROOT "${_VMRP_FLUID_BUNDLE}" CACHE PATH "Bundled FluidSynth (thirdparty/fluidsynth/<ABI>)" FORCE)
        endif()
    endif()
    if(VMRP_FLUIDSYNTH_ROOT AND EXISTS "${VMRP_FLUIDSYNTH_ROOT}/include/fluidsynth.h" AND EXISTS "${VMRP_FLUIDSYNTH_ROOT}/lib/libfluidsynth.so")
        set(_VMRP_MIDI_LINK_NATIVE 1)
        set(VMRP_MIDI_AUDIO_SRC ${VMRP_DIR}/harmony_midi_audio_native.c)
    else()
        message(WARNING "VMRP_NATIVE_MIDI=ON 但未找到 ${NATIVERENDER_ROOT_PATH}/thirdparty/fluidsynth/${_VMRP_OHOS_ARCH}（或 VMRP_FLUIDSYNTH_ROOT 无效），本 ABI 将使用 stub 且不链接 FluidSynth")
        set(VMRP_MIDI_AUDIO_SRC ${VMRP_DIR}/harmony_midi_audio_stub.c)
    endif()
else()
    set(VMRP_MIDI_AUDIO_SRC ${VMRP_DIR}/harmony_midi_audio_stub.c)
endif()

set(VMRP_SOURCES
    ${VMRP_DIR}/vmrp.c
    ${VMRP_DIR}/bridge.c
    ${VMRP_DIR}/memory.c
    ${VMRP_DIR}/fileLib.c
    ${VMRP_DIR}/network.c
    ${VMRP_DIR}/rbtree.c
    ${VMRP_DIR}/utils.c
    ${VMRP_DIR}/platform_harmony.c
    ${VMRP_DIR}/gbk_conv.c
    ${VMRP_MIDI_AUDIO_SRC}
)

include_directories(
    ${NATIVERENDER_ROOT_PATH}
    ${NATIVERENDER_ROOT_PATH}/include
    ${VMRP_DIR}
    ${VMRP_DIR}/header
    ${UNICORN_INC}
)

find_library(MRP_EGL_LIB EGL)
find_library(MRP_GLES2_LIB GLESv2)

add_library(entry SHARED
    napi_init.cpp
    mrp_gles_renderer.cpp
    ${VMRP_SOURCES}
)

target_compile_definitions(entry PRIVATE
    __HARMONYOS__
    UNICORN_HAS_ARM
    VMRP
)

target_link_libraries(entry PUBLIC
    libace_napi.z.so
    libace_ndk.z.so
    libnative_window.so
    libhilog_ndk.z.so
    unicorn
    libz.so
    ${MRP_EGL_LIB}
    ${MRP_GLES2_LIB}
)

if(_VMRP_MIDI_LINK_NATIVE)
    target_compile_definitions(entry PRIVATE VMRP_HAVE_NATIVE_MIDI=1)
    target_include_directories(entry PRIVATE ${VMRP_FLUIDSYNTH_ROOT}/include)
    target_link_directories(entry PRIVATE ${VMRP_FLUIDSYNTH_ROOT}/lib)
    target_link_libraries(entry PRIVATE libohaudio.so libfluidsynth.so)
    # 必须拷到与 libentry.so 相同的 CMake 输出目录：hvigor 从该目录收集进 HAP 的 libs/arm64，
    # 仅放 src/main/libs 时在本工程链路上可能不会与 libentry 合并，导致仍报缺 libfluidsynth.so.3。
    # lycium 等布局常把 libinstpatch 放在 lib64/，仅扫 lib/ 会导致运行时缺 libinstpatch-1.0.so.2
    file(GLOB _VMRP_FLUID_PREBUILT_SO
        "${VMRP_FLUIDSYNTH_ROOT}/lib/*.so"
        "${VMRP_FLUIDSYNTH_ROOT}/lib/*.so.*"
        "${VMRP_FLUIDSYNTH_ROOT}/lib64/*.so"
        "${VMRP_FLUIDSYNTH_ROOT}/lib64/*.so.*"
    )
    foreach(_src IN LISTS _VMRP_FLUID_PREBUILT_SO)
        get_filename_component(_fname "${_src}" NAME)
        add_custom_command(TARGET entry POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_src}"
                "$<TARGET_FILE_DIR:entry>/${_fname}"
            COMMENT "Package native dep next to libentry.so: ${_fname}"
        )
    endforeach()
endif()
