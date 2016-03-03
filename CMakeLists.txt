cmake_minimum_required(VERSION 3.3)

project(cds CXX C)

find_package(Boost 1.59.0 REQUIRED QUIET COMPONENTS filesystem system)
find_package(Threads)

if(NOT TARGET utfcpp)
  add_subdirectory("../utfcpp" "${CMAKE_BINARY_DIR}/utfcpp")
endif()

if(NOT TARGET stlab-libraries)
  add_subdirectory("../libraries" "${CMAKE_BINARY_DIR}/stlab-libraries")
endif()

add_executable(
  cds
  main.cpp
  )

target_compile_options(
  cds
  PRIVATE
  $<$<CXX_COMPILER_ID:GNU>:-std=c++14>
  # $<$<CXX_COMPILER_ID:GNU>:-pg -fprofile-generate>
  # $<$<CXX_COMPILER_ID:Clang>:-O2 -fprofile-instr-generate -v>
  $<$<CXX_COMPILER_ID:Clang>:-std=c++14 -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic>
  $<$<CXX_COMPILER_ID:Clang>:-Wno-exit-time-destructors -Wno-padded -Wno-documentation-unknown-command -Wno-documentation -Wno-weak-vtables -Wno-old-style-cast>
  )

target_link_libraries(
  cds
  PRIVATE
  utfcpp
  stlab-libraries
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  Threads::Threads
  # $<$<CXX_COMPILER_ID:GNU>:gcov>
  # $<$<CXX_COMPILER_ID:GNU>:-pg>
  )

target_include_directories(
  cds
  SYSTEM
  PRIVATE
  ${Boost_INCLUDE_DIRS}
  )
