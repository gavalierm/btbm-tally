# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/Users/gavo/Projects/VYVOJ/arduino/btbm-tally/esp-idf/components/bootloader/subproject"
  "/Users/gavo/Projects/VYVOJ/arduino/btbm-tally/build/bootloader"
  "/Users/gavo/Projects/VYVOJ/arduino/btbm-tally/build/bootloader-prefix"
  "/Users/gavo/Projects/VYVOJ/arduino/btbm-tally/build/bootloader-prefix/tmp"
  "/Users/gavo/Projects/VYVOJ/arduino/btbm-tally/build/bootloader-prefix/src/bootloader-stamp"
  "/Users/gavo/Projects/VYVOJ/arduino/btbm-tally/build/bootloader-prefix/src"
  "/Users/gavo/Projects/VYVOJ/arduino/btbm-tally/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/gavo/Projects/VYVOJ/arduino/btbm-tally/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/gavo/Projects/VYVOJ/arduino/btbm-tally/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
