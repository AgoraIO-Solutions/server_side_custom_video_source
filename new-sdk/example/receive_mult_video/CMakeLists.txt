project(DefaultSamples)

# Common file parsers
file(GLOB FILE_PARSER_CPP_FILES
     "${PROJECT_SOURCE_DIR}/../common/file_parser/helper_h264_parser.cpp"
     "${PROJECT_SOURCE_DIR}/../common/file_parser/helper_aac_parser.cpp")

# Build sample_receive_yuv_from_multi_user
file(GLOB SAMPLE_RECEIVE_YUV_FROM_MULTI_USER_CPP_FILES
     "${PROJECT_SOURCE_DIR}/sample_receive_yuv_from_multi_user.cpp"
     "${PROJECT_SOURCE_DIR}/../common/*.cpp")
add_executable(sample_receive_yuv_from_multi_user ${SAMPLE_RECEIVE_YUV_FROM_MULTI_USER_CPP_FILES})