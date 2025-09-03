cmake_minimum_required(VERSION 3.16)
project(flirmulticamera_examples)

set(CMAKE_CXX_STANDARD 17)

# If running standalone, find the necessary packages
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    find_package(OpenCV REQUIRED)
    find_package(fmt REQUIRED)
    find_package(cpp_utils REQUIRED)
endif()

# Assume the main library is in a directory relative to this one if running standalone
if(NOT TARGET flirmulticamera)
    add_library(flirmulticamera STATIC IMPORTED)
    set_property(TARGET flirmulticamera PROPERTY IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../src/libflirmulticamera.a)
    set_property(TARGET flirmulticamera PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/../include)
endif()

# Paths to source files (use PROJECT_SOURCE_DIR if running standalone)
set(EXAMPLE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/apps)

# Add example executables
add_executable(stream_synced_videos ${EXAMPLE_SRC_DIR}/stream_synced_videos.cpp)
target_link_libraries(stream_synced_videos flirmulticamera)

add_executable(record_synchronized_videos ${EXAMPLE_SRC_DIR}/synchronized_videos.cpp)
target_link_libraries(record_synchronized_videos flirmulticamera)

add_executable(record_synchronized_frame ${EXAMPLE_SRC_DIR}/synchronized_frame.cpp)
target_link_libraries(record_synchronized_frame flirmulticamera)


# install python bindings
execute_process(
  COMMAND python3 -c "import site; print(site.getsitepackages()[0])"
  OUTPUT_VARIABLE PYTHON_SITE_PACKAGES
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

install(TARGETS pyflircam
  LIBRARY DESTINATION ${PYTHON_SITE_PACKAGES}
)