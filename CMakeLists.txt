cmake_minimum_required(VERSION 3.16)
project(flirmulticamera)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)
# set(CMAKE_BUILD_TYPE Debug)

link_directories(/usr/local/lib)

if(POLICY CMP0146)
  cmake_policy(SET CMP0146 OLD)
endif()

find_package(OpenCV ${OPENCV_VERSION} REQUIRED)
find_package(cpp_utils REQUIRED)
find_package(pybind11 REQUIRED)
find_package(fmt REQUIRED)

#set(RAPIDJSON_INCLUDE_DIRS "~/libs/rapidjson/include")
set(SPINNAKER_INCLUDE_DIRS "/opt/spinnaker/include/")
set(SPINNAKER_LIBRARIES "/opt/spinnaker/lib/libSpinnaker.so")

# video encoding stuff
find_library(AVCODEC_LIBRARY avcodec)
find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
find_library(AVCODEC_LIBRARY avcodec)
find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
find_library(AVFORMAT_LIBRARY avformat)
find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
find_library(AVUTIL_LIBRARY avutil)
find_path(AVDEVICE_INCLUDE_DIR libavdevice/avdevice.h)
find_library(AVDEVICE_LIBRARY avdevice)
find_path(AVSWSCALE_INCLUDE_DIR libswscale/swscale.h)
find_library(AVSWSCALE_LIBRARY swscale)
##################################################################################
# BEGIN: const header cam serial numbers
##################################################################################
set(CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cfg)
set(MASTER_LINE $ENV{FLIR_MASTER_LINE})
set(SLAVE_LINE $ENV{FLIR_SLAVE_LINE})
set(FLIR_CAMERA_SERIAL_NUMBERS $ENV{FLIR_CAMERA_SERIAL_NUMBERS})
set(FLIR_CAMERA_COUNT $ENV{FLIR_CAMERA_COUNT})
set(FLIR_MASTER_CAM_SERIAL $ENV{FLIR_MASTER_CAM_SERIAL})
set(FLIR_TOP_CAM_SERIAL $ENV{FLIR_TOP_CAM_SERIAL})
string(REPLACE "," ";" SERIAL_NUMBERS ${FLIR_CAMERA_SERIAL_NUMBERS})

set(GENERATED_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/hardware_constants.h")
file(WRITE ${GENERATED_HEADER} "// This file is auto-generated. Do not edit.\n")
file(APPEND ${GENERATED_HEADER} "#pragma once\n\n")
file(APPEND ${GENERATED_HEADER} "#include <array>\n#include <string_view>\n\n")
file(APPEND ${GENERATED_HEADER} "namespace ${PROJECT_NAME} {\n\n")
file(APPEND ${GENERATED_HEADER} "constexpr std::string_view CONFIG_DIR = \"${CONFIG_DIR}\";\n")
file(APPEND ${GENERATED_HEADER} "constexpr std::string_view GLOBAL_CONST_MASTER_LINE = \"${MASTER_LINE}\";\n")
file(APPEND ${GENERATED_HEADER} "constexpr std::string_view GLOBAL_CONST_SLAVE_LINE = \"${SLAVE_LINE}\";\n")
file(APPEND ${GENERATED_HEADER} "constexpr std::size_t GLOBAL_CONST_NCAMS = ${FLIR_CAMERA_COUNT};\n")
file(
  APPEND
  ${GENERATED_HEADER}
  "constexpr std::array<std::string_view, ${FLIR_CAMERA_COUNT}> GLOBAL_CONST_CAMERA_SERIAL_NUMBERS = {\n")

foreach(val IN LISTS SERIAL_NUMBERS)
    file(APPEND ${GENERATED_HEADER} "    \"${val}\",\n")
endforeach()

file(APPEND ${GENERATED_HEADER} "};\n")
file(APPEND ${GENERATED_HEADER} "constexpr std::string_view GLOBAL_CONST_MASTER_CAM_SERIAL = \"${FLIR_MASTER_CAM_SERIAL}\";\n")
file(APPEND ${GENERATED_HEADER} "constexpr std::string_view GLOBAL_CONST_TOP_CAM_SERIAL = \"${FLIR_TOP_CAM_SERIAL}\";\n")
file(APPEND ${GENERATED_HEADER} "}\n")
##################################################################################
# END: const header cam serial numbers
##################################################################################

message(STATUS "cpp_utils include dir: ${cpp_utils_INCLUDE_DIRS}")

set(include_dirs
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${SPINNAKER_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
  ${cpp_utils_INCLUDE_DIRS}
  ${AVCODEC_INCLUDE_DIR}
  ${AVFORMAT_INCLUDE_DIR}
  ${AVUTIL_INCLUDE_DIR}
  ${AVDEVICE_INCLUDE_DIR}
  ${AVSWSCALE_INCLUDE_DIR}
)

set(libraries
  ${SPINNAKER_LIBRARIES}
  ${OpenCV_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
  -lpthread
  ${AVCODEC_LIBRARY}
  ${AVFORMAT_LIBRARY}
  ${AVUTIL_LIBRARY}
  ${AVDEVICE_LIBRARY}
  ${AVSWSCALE_LIBRARY}
  fmt::fmt
  cpp_utils::cpp_utils
)

# ============ LIBRARY ============
add_library(${PROJECT_NAME} STATIC
  "src/config_parser.cpp"
  "src/FlirCamera.cpp"
  "src/OnlineEncoder.cpp"
  "src/videoIO.cpp"
)

target_link_libraries(${PROJECT_NAME} PUBLIC ${libraries})
target_include_directories(${PROJECT_NAME} PUBLIC ${include_dirs})

# ============ INSTALLATION ============
install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  INCLUDES DESTINATION include
)
# installs the export file (the “recipe”) created in the previous step into a standard location within your install tree (for find_package)
install(EXPORT ${PROJECT_NAME}Targets
  # NAMESPACE ${PROJECT_NAME}::
  DESTINATION lib/cmake/${PROJECT_NAME}
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION lib/cmake/${PROJECT_NAME}
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  VERSION $DETECTION_INFERENCE_VERSION
  COMPATIBILITY AnyNewerVersion
)
# build tree usage
export(EXPORT ${PROJECT_NAME}Targets
  FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake"
  # NAMESPACE ${PROJECT_NAME}::
)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  "include(\"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake\")"
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  DESTINATION lib/cmake/${PROJECT_NAME}
)
# Installs all header files (*.h) from your include/ folder to: /usr/local/include/${PROJECT_NAME}
install(DIRECTORY src/ DESTINATION /usr/local/include/${PROJECT_NAME}
  FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

# install python bindings
execute_process(
  COMMAND python3 -c "import site; print(site.getsitepackages()[0])"
  OUTPUT_VARIABLE PYTHON_SITE_PACKAGES
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

install(TARGETS pyflircam
  LIBRARY DESTINATION ${PYTHON_SITE_PACKAGES}
)

# Optionally include examples
if(EXAMPLES_ENABLED)
  add_subdirectory(examples)
endif()