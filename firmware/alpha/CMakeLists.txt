# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctlycmake_minimum_required(VERSION 3.5)
cmake_minimum_required(VERSION 3.5)
add_compile_options(-fdiagnostics-color=always)

if(NOT DEFINED IDF_TARGET)
    set(IDF_TARGET "esp32")
endif()

set(PROJECT_VER "0.0.3")

# include(${ESPNOW_PATH}/component.cmake)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# vscp and vscp-firmware sources, fetched when the project is configured.
# Only their files are used (see main/CMakeLists.txt); SOURCE_SUBDIR points to
# a folder without a CMakeLists.txt so their own builds are not added.
# To move to another commit, change the commit and the SHA-256 of its tarball.
include(FetchContent)

# vscp-firmware: a commit from before common/ was renamed to src/ (0eaa9f5).
# ab829cd fixes a typo that kept common/vscp-link-protocol.c from compiling.
set(VSCP_FIRMWARE_COMMIT ab829cdeee687fedbecd03dd49305c669742e137)
FetchContent_Declare(vscp_firmware
  URL https://github.com/grodansparadis/vscp-firmware/archive/${VSCP_FIRMWARE_COMMIT}.tar.gz
  URL_HASH SHA256=850340363f2c0ace73f2f139d891ad415e94c08307f3da069c2d85150df03085
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR no-cmake-build
)

set(VSCP_COMMIT d75d4f5c6c097a1dca841b63727c79c522b701ce)
FetchContent_Declare(vscp
  URL https://github.com/grodansparadis/vscp/archive/${VSCP_COMMIT}.tar.gz
  URL_HASH SHA256=709763c16c995b65787e72947f9c3f45a03a404c708286c7cad00b53dcaedbb5
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR no-cmake-build
)

FetchContent_MakeAvailable(vscp_firmware vscp)

string(REGEX REPLACE ".*/\(.*\)" "\\1" CURDIR ${CMAKE_CURRENT_SOURCE_DIR})
project(${CURDIR})

if(DEFINED ENV{PROJECT_NAME})
   set(PROJECT_NAME $ENV{PROJECT_NAME})
else()
  set(PROJECT_NAME "vscp_espnow_alpha")
endif(DEFINED ENV{PROJECT_NAME})

project(${PROJECT_NAME})

set(EXTRA_COMPONENT_DIRS "../common/components/")

git_describe(PROJECT_VERSION ${COMPONENT_DIR})
message("Project commit: " ${PROJECT_VERSION})
