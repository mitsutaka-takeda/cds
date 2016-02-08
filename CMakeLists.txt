cmake_minimum_required(VERSION 3.3)

project(cds CXX C)

find_package(Boost 1.59.0 REQUIRED QUIET COMPONENTS filesystem system)

if(NOT TARGET RxCpp)
  add_subdirectory("../RxCpp" "${CMAKE_BINARY_DIR}/RxCpp")
endif()

add_executable(
  cds
  main.cpp
  )

target_compile_options(
  cds
  PRIVATE
  $<$<CXX_COMPILER_ID:GNU>:-std=c++14>
  $<$<CXX_COMPILER_ID:Clang>:-std=c++14 -Werror -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic>
  $<$<CXX_COMPILER_ID:Clang>:-Wno-exit-time-destructors -Wno-padded -Wno-documentation-unknown-command -Wno-documentation -Wno-weak-vtables -Wno-old-style-cast>
  )

target_link_libraries(
  cds
  PRIVATE
  RxCpp
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  )

target_include_directories(
  cds
  SYSTEM
  PRIVATE
  ${Boost_INCLUDE_DIRS}
  )
