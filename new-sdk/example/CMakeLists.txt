cmake_minimum_required(VERSION 2.4)

project(AgoraSDKDemo)

if("${MEM_CHECK}" STREQUAL "true")
  set(sanitizer -fsanitize=address)
endif()

if("${TARGET_OS}" STREQUAL "linux")
  add_definitions("-DTARGET_OS_LINUX")
  set(lib_sdk "agora_rtc_sdk")
  set(lib_thread "pthread" "dl" "rt")

  # Avoid to use features above c++11 in Linux
  set(CMAKE_CXX_STANDARD 11)
else()
  add_definitions("-DTARGET_OS_ANDROID")
  set(lib_sdk "agora-rtsa-sdk")
  set(lib_thread "stdc++" "z" "log")
endif()

set(AGORA_MEDIA_SDK_DIR ${CMAKE_SOURCE_DIR}/../sdk)
set(AGORA_MEDIA_SDK_HEADER_DIR
    ${CMAKE_SOURCE_DIR}/../sdk)
set(AGORA_SDK_DEMO_DIR ${CMAKE_SOURCE_DIR})

# prepare_libs(Gtest)

# Set common flags
set(CMAKE_DEBUG_POSTFIX "")
set(CMAKE_RELEASE_POSTFIX "")
set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer ${sanitizer} -Werror")

# Prepare third-party libs    ###
set(third_party_path "${CMAKE_SOURCE_DIR}/../third_party/")

# include header path
include_directories("${third_party_path}/include"
                    "${third_party_path}/http_parser/include"
                    "${third_party_path}/json_parser/include"
                    ${AGORA_MEDIA_SDK_HEADER_DIR}/low_level_api/include)

# Link common libs
link_libraries(${lib_sdk} ${lib_thread})

link_directories(${AGORA_MEDIA_SDK_DIR} ${third_party_path}/lib/)
link_directories(${third_party_path}/http_parser/lib/)


set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin/)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin/)

# include header path
include_directories(${AGORA_SDK_DEMO_DIR})

# Build apps
if("${SDK_LICENSE_ENABLE}" STREQUAL "on")
endif()
add_subdirectory(h264_pcm)
add_subdirectory(encrypt)
add_subdirectory(h264_opus)
add_subdirectory(string_uid)
add_subdirectory(yuv_pcm)
add_subdirectory(mixed_audio)
add_subdirectory(receive_mult_video)
add_subdirectory(send_dual_video_stream)
