cmake_minimum_required(VERSION 3.21)
project(DmmPhotoDecor VERSION 0.1.0 LANGUAGES CXX)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "GEODE_SDK environment variable is not set. Install the Geode SDK for v6.0.0-alpha.1 first.")
endif()

add_subdirectory("$ENV{GEODE_SDK}" "${CMAKE_CURRENT_BINARY_DIR}/geode")

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})
setup_geode_mod(${PROJECT_NAME})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE comdlg32 gdiplus)
else()
    message(FATAL_ERROR "Dmm Photo Decor v0.1.0 currently targets Windows x64 only.")
endif()
