project(http_parser)

include_directories(${PROJECT_SOURCE_DIR}/include)
link_directories(${PROJECT_SOURCE_DIR}/lib)

file(GLOB_RECURSE ${PROJECT_NAME}_files
    ${PROJECT_SOURCE_DIR}/src/*.c
    )

MESSAGE(project_files " " ${${PROJECT_NAME}_files})

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

add_library(${PROJECT_NAME} STATIC ${${PROJECT_NAME}_files})
