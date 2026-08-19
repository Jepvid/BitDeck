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

add_library(bitdeckcore STATIC
    src/core/resource.cpp
    src/core/types/background.cpp
    src/core/types/sequence.cpp
    src/core/types/texture.cpp
    src/core/image_codec.cpp
    src/core/texture_conversion.cpp
    src/games/bk64_conventions.cpp
)
target_include_directories(bitdeckcore PUBLIC src)
target_link_libraries(bitdeckcore PUBLIC PNG::PNG JPEG::JPEG)

# .otr (MPQ) / .o2r (zip) archive layer.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(STORM_SKIP_INSTALL ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/StormLib)
find_package(libzip CONFIG REQUIRED)

add_library(bitdeckarchive STATIC
    src/archive/arc.cpp
    src/archive/extraction_safety.cpp
)
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

# App shell: theme, reusable widgets, navigation, staging model, worker helper.
add_library(bitdeckapp STATIC
    src/app/theme.cpp
    src/app/page_frame.cpp
    src/app/option_card.cpp
    src/app/numpad.cpp
    src/app/staging_model.cpp
    src/app/ephemeral_bar.cpp
    src/app/main_window.cpp
    src/app/background_worker.cpp
    src/app/archive_generator.cpp
    src/app/texture_manifest_json.cpp
    src/app/game_conventions_registry.cpp
    src/app/texture_extraction.cpp
)
target_include_directories(bitdeckapp PUBLIC src ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(bitdeckapp PUBLIC Qt6::Widgets bitdeckcore bitdeckarchive)

qt_add_executable(BitDeck
    src/main.cpp
    src/features/common/not_implemented_page.cpp
    src/features/home/home_page.cpp
    src/features/create_selection/create_selection_page.cpp
    src/features/create_custom/create_custom_page.cpp
    src/features/create_custom_sequences/create_custom_sequences_page.cpp
    src/features/create_replace_textures/create_replace_textures_page.cpp
    src/features/inspect_otr/inspect_otr_page.cpp
    src/features/debug_selection/debug_selection_page.cpp
    src/features/debug_convert_textures/debug_convert_textures_page.cpp
    src/features/finish/finish_panel.cpp
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
