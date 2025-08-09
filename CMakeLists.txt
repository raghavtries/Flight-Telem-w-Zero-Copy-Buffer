# Flight Telemetry Ingestor — CMake build
cmake_minimum_required(VERSION 3.14)
project(flight_telemetry_ingestor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(telemetry_headers INTERFACE)
target_include_directories(telemetry_headers INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
