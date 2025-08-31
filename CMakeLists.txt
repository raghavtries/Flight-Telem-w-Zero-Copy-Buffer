# Flight Telemetry Ingestor — CMake build
cmake_minimum_required(VERSION 3.14)
project(flight_telemetry_ingestor LANGUAGES CXX)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TELEMETRY_MARCH_NATIVE "Pass -march=native for CRC/intrinsics tuning (dev only)" OFF)

add_library(telemetry_core STATIC
  src/buffer_pool.cpp
  src/wal.cpp
  src/validate.cpp
  src/receiver.cpp
  src/writer.cpp
)
target_include_directories(telemetry_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(telemetry_core PUBLIC Threads::Threads)
target_compile_options(telemetry_core PRIVATE
  -Wall -Wextra -Wpedantic
)
if(TELEMETRY_MARCH_NATIVE)
  target_compile_options(telemetry_core PRIVATE -march=native)
endif()

# POSIX + Linux O_DIRECT / macOS — no extra link libs required for core
if(UNIX AND NOT APPLE)
  target_compile_definitions(telemetry_core PRIVATE TELEMETRY_TARGET_LINUX=1)
endif()

add_executable(telemetry_ingestor src/main.cpp)
target_link_libraries(telemetry_ingestor PRIVATE telemetry_core)
target_compile_options(telemetry_ingestor PRIVATE -Wall -Wextra -Wpedantic)

add_executable(udp_blast tools/udp_blast.cpp)
target_include_directories(udp_blast PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(udp_blast PRIVATE Threads::Threads)
target_compile_options(udp_blast PRIVATE -Wall -Wextra -Wpedantic)
