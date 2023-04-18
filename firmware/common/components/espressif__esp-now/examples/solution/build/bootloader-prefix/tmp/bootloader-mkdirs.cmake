# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/usr/local/src/esp/esp-idf-v5.0.1/components/bootloader/subproject"
  "/usr/local/src/esp/esp-now/examples/solution/build/bootloader"
  "/usr/local/src/esp/esp-now/examples/solution/build/bootloader-prefix"
  "/usr/local/src/esp/esp-now/examples/solution/build/bootloader-prefix/tmp"
  "/usr/local/src/esp/esp-now/examples/solution/build/bootloader-prefix/src/bootloader-stamp"
  "/usr/local/src/esp/esp-now/examples/solution/build/bootloader-prefix/src"
  "/usr/local/src/esp/esp-now/examples/solution/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/usr/local/src/esp/esp-now/examples/solution/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/usr/local/src/esp/esp-now/examples/solution/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
