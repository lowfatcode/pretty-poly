cmake_minimum_required(VERSION 3.12)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wnarrowing ")
set(CMAKE_C_STANDARD 17)

project(examples C)

include_directories(
  .
)

add_subdirectory(examples)
