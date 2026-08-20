cmake_minimum_required(VERSION 3.21)
project(BitDeck LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Headless core: OTR/O2R binary format, texture codec, per-game conventions.
# No Qt dependency.
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)

file(GLOB_RECURSE BITDECKCORE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/games/*.cpp
)
add_library(bitdeckcore STATIC ${BITDECKCORE_SOURCES})
target_include_directories(bitdeckcore PUBLIC src)
target_link_libraries(bitdeckcore PUBLIC PNG::PNG JPEG::JPEG)

# .otr (MPQ) / .o2r (zip) archive layer.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(STORM_SKIP_INSTALL ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/StormLib)
find_package(libzip CONFIG REQUIRED)

file(GLOB_RECURSE BITDECKARCHIVE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/archive/*.cpp)
add_library(bitdeckarchive STATIC ${BITDECKARCHIVE_SOURCES})
target_include_directories(bitdeckarchive PUBLIC src)
target_include_directories(bitdeckarchive PRIVATE third_party/StormLib/src)
target_link_libraries(bitdeckarchive PUBLIC bitdeckcore storm libzip::zip)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

# Version/git info, generated at configure time (mirrors Retro's build.sh).
find_package(Git QUIET)
set(BITDECK_VERSION "0.1.0-dev")
if(Git_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE BITDECK_GIT_BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE BITDECK_GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${GIT_EXECUTABLE} show -s --format=%ci
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE BITDECK_GIT_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
else()
    set(BITDECK_GIT_BRANCH "unknown")
    set(BITDECK_GIT_COMMIT "unknown")
    set(BITDECK_GIT_DATE "unknown")
endif()
configure_file(src/app/version.h.in ${CMAKE_BINARY_DIR}/generated/bitdeck/version.h @ONLY)

# App shell: theme, reusable widgets, shell/mode plumbing, staging model, worker helper.
file(GLOB_RECURSE BITDECKAPP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/features/*.cpp
)
add_library(bitdeckapp STATIC ${BITDECKAPP_SOURCES})
target_include_directories(bitdeckapp PUBLIC src ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(bitdeckapp PUBLIC Qt6::Widgets bitdeckcore bitdeckarchive)

qt_add_executable(BitDeck
    WIN32 MACOSX_BUNDLE
    src/main.cpp
)

target_link_libraries(BitDeck PRIVATE Qt6::Widgets bitdeckcore bitdeckarchive bitdeckapp)

enable_testing()
add_executable(bitdeckcore_tests tests/core_tests.cpp)
target_link_libraries(bitdeckcore_tests PRIVATE bitdeckcore)
add_test(NAME bitdeckcore_tests COMMAND bitdeckcore_tests)

add_executable(image_codec_tests tests/image_codec_tests.cpp)
target_link_libraries(image_codec_tests PRIVATE bitdeckcore JPEG::JPEG)
add_test(NAME image_codec_tests COMMAND image_codec_tests)

add_executable(texture_conversion_tests tests/texture_conversion_tests.cpp)
target_link_libraries(texture_conversion_tests PRIVATE bitdeckcore JPEG::JPEG)
add_test(NAME texture_conversion_tests COMMAND texture_conversion_tests)

add_executable(gfx_tlut_scan_tests tests/gfx_tlut_scan_tests.cpp)
target_link_libraries(gfx_tlut_scan_tests PRIVATE bitdeckcore)
add_test(NAME gfx_tlut_scan_tests COMMAND gfx_tlut_scan_tests)

add_executable(path_tree_tests tests/path_tree_tests.cpp)
target_link_libraries(path_tree_tests PRIVATE bitdeckcore)
add_test(NAME path_tree_tests COMMAND path_tree_tests)

add_executable(folder_scan_tests tests/folder_scan_tests.cpp)
target_link_libraries(folder_scan_tests PRIVATE bitdeckcore)
add_test(NAME folder_scan_tests COMMAND folder_scan_tests)

add_executable(bitdeckarchive_tests tests/archive_tests.cpp)
target_link_libraries(bitdeckarchive_tests PRIVATE bitdeckarchive)
add_test(NAME bitdeckarchive_tests COMMAND bitdeckarchive_tests)

add_executable(app_worker_tests tests/app_worker_tests.cpp)
target_link_libraries(app_worker_tests PRIVATE bitdeckapp)
add_test(NAME app_worker_tests COMMAND app_worker_tests)

add_executable(archive_generator_tests tests/archive_generator_tests.cpp)
target_link_libraries(archive_generator_tests PRIVATE bitdeckapp bitdeckarchive)
add_test(NAME archive_generator_tests COMMAND archive_generator_tests)

add_executable(texture_extraction_tests tests/texture_extraction_tests.cpp)
target_link_libraries(texture_extraction_tests PRIVATE bitdeckapp bitdeckarchive)
add_test(NAME texture_extraction_tests COMMAND texture_extraction_tests)
