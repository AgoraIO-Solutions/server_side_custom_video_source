project(DefaultSamples)

# Common file parsers
file(GLOB FILE_PARSER_CPP_FILES
     "${PROJECT_SOURCE_DIR}/../common/file_parser/helper_h264_parser.cpp"
     "${PROJECT_SOURCE_DIR}/../common/file_parser/helper_aac_parser.cpp")

# Opus file parser
file(GLOB OPUS_FILE_PARSER_CPP_FILES
     "${PROJECT_SOURCE_DIR}/../common/file_parser/helper_opus_parser.cpp")

# Build sample_send_h264_opus
file(GLOB SAMPLE_SEND_H264_OPUS_CPP_FILES
     "${PROJECT_SOURCE_DIR}/sample_send_h264_opus.cpp"
     "${PROJECT_SOURCE_DIR}/../common/*.cpp")
add_executable(
  sample_send_h264_opus ${SAMPLE_SEND_H264_OPUS_CPP_FILES}
                        ${FILE_PARSER_CPP_FILES} ${OPUS_FILE_PARSER_CPP_FILES})
target_compile_definitions(sample_send_h264_opus PRIVATE __SUPPORT_OPUS__)
target_link_libraries(sample_send_h264_opus opusfile_static)