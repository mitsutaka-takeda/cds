cmake_minimum_required(VERSION 3.3)

project(cds CXX C)

find_package(Boost 1.59.0 REQUIRED QUIET COMPONENTS filesystem system)
find_package(Threads)

if(NOT TARGET RxCpp)
  add_subdirectory("../RxCpp" "${CMAKE_BINARY_DIR}/RxCpp")
endif()

if(NOT TARGET utfcpp)
  add_subdirectory("../utfcpp" "${CMAKE_BINARY_DIR}/utfcpp")
endif()

add_executable(
  cds
  main.cpp
  rx-threadpool.hpp
  rx-threadpool.cpp
  task-system.hpp
  )

target_compile_options(
  cds
  PRIVATE
  $<$<CXX_COMPILER_ID:GNU>:-std=c++14>
  $<$<CXX_COMPILER_ID:Clang>:-std=c++14 -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic>
  $<$<CXX_COMPILER_ID:Clang>:-Wno-exit-time-destructors -Wno-padded -Wno-documentation-unknown-command -Wno-documentation -Wno-weak-vtables -Wno-old-style-cast>
  )

target_link_libraries(
  cds
  PRIVATE
  RxCpp
  utfcpp
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  Threads::Threads
  )

target_include_directories(
  cds
  SYSTEM
  PRIVATE
  ${Boost_INCLUDE_DIRS}
  )

# Comapile with clang && libstdc++
# The executable built with clang will crash. Maybe it's because of that Boost is built with gcc.
# CC=clang CXX=clang++ cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBOOST_ROOT=path/to/boost_1_59_0 -DCMAKE_CXX_FLAGS=-stdlib=libstdc++ -DCMAKE_EXE_LINKER_FLAGS=-stdlib=libstdc++ /path/to/cds/ &
# Compile with gcc
# CC=gcc CXX=g++ cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBOOST_ROOT=/home/mtakeda/etc/boost_1_59_0/ /home/mtakeda/src/cpp/cds/ &
