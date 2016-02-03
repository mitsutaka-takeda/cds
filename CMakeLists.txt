cmake_minimum_required(VERSION 3.3)

project(cds CXX C)

find_package(Boost 1.59.0 REQUIRED QUIET)

if(NOT TARGET RxCpp)
  add_subdirectory("../RxCpp" "${CMAKE_BINARY_DIR}/RxCpp")
endif()

add_executable(
  cds
  main.cpp
  )

target_link_libraries(
  cds
  RxCpp
  )
