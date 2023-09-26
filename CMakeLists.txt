cmake_minimum_required(VERSION 3.12)

set(CMAKE_C_STANDARD 17)

project(examples C)

include_directories(
  .
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")

add_subdirectory(examples)
