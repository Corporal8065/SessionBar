cmake_minimum_required(VERSION 3.22)
project(SessionBar VERSION 0.2.0 LANGUAGES C CXX)
# Static MSVC runtime (/MT) so a built SessionBar.exe runs on any
# Windows 10/11 PC with NO Visual C++ Redistributable installed
# (MeterBar v0.15 rule). Must be set before any target is created
# (CMP0091). MSVC-only; inert on other toolchains.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
# C++20: the session core is built on std::chrono's IANA time-zone
# database (locate_zone / zoned_time) — real DST rules, no hand-rolled
# offsets. (The family apps are C++17; this one genuinely needs 20.)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(SESSIONBAR_BUILD_APP "Build the JUCE GUI app" OFF)

# ---- Pure core: session math, zero external deps ----
add_library(sessionbar_core INTERFACE)
target_include_directories(sessionbar_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/core/include)
if(UNIX AND NOT APPLE)
  target_link_libraries(sessionbar_core INTERFACE m)
endif()

enable_testing()
add_executable(test_core tests/test_core.cpp)
target_link_libraries(test_core PRIVATE sessionbar_core)
add_test(NAME core COMMAND test_core)

# ---- JUCE app ----
if(SESSIONBAR_BUILD_APP)
  include(FetchContent)
  # URL (not git): the Windows build PC has no git installed; CMake can
  # download+extract a release archive itself (MeterBar kit fix v0.5).
  FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(JUCE)
  add_subdirectory(app)
endif()
