# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/runner/esp/idf/v5.2/esp-idf/components/bootloader/subproject"
  "/home/runner/work/Esp32Vault/Esp32Vault/_codeql_build_dir/bootloader"
  "/home/runner/work/Esp32Vault/Esp32Vault/_codeql_build_dir/bootloader-prefix"
  "/home/runner/work/Esp32Vault/Esp32Vault/_codeql_build_dir/bootloader-prefix/tmp"
  "/home/runner/work/Esp32Vault/Esp32Vault/_codeql_build_dir/bootloader-prefix/src/bootloader-stamp"
  "/home/runner/work/Esp32Vault/Esp32Vault/_codeql_build_dir/bootloader-prefix/src"
  "/home/runner/work/Esp32Vault/Esp32Vault/_codeql_build_dir/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/runner/work/Esp32Vault/Esp32Vault/_codeql_build_dir/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/runner/work/Esp32Vault/Esp32Vault/_codeql_build_dir/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
