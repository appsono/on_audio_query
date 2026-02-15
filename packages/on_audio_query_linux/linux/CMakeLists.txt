cmake_minimum_required(VERSION 3.10)
set(PROJECT_NAME "on_audio_query_linux")
project(${PROJECT_NAME} LANGUAGES CXX)

# This value is used when generating builds using this plugin 
# => so it must not be changed
set(PLUGIN_NAME "${PROJECT_NAME}_plugin")

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

# JSON library 
include(FetchContent)

# Set policy for timestamp behavior
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

# Check for ffprobe (at build time)
find_program(FFPROBE_EXECUTABLE ffprobe)
if(NOT FFPROBE_EXECUTABLE)
    message(WARNING "ffprobe not found. Runtime extraction will fail without ffprobe installed.")
endif()

# Plugin sources
add_library(${PLUGIN_NAME} SHARED
  # Main plugin
  "src/on_audio_query_linux_plugin.cc"

  # Core
  "src/core/database_manager.cc"
  "src/core/ffprobe_extractor.cc"
  "src/core/thread_pool.cc"

  # Scanner
  "src/scanner/file_scanner.cc"
  "src/scanner/incremental_scanner.cc"
  "src/scanner/scan_coordinator.cc"

  # Queries
  "src/queries/base_query.cc"
  "src/queries/audio_query.cc"
  "src/queries/album_query.cc"
  "src/queries/artist_query.cc"
  "src/queries/genre_query.cc"
  "src/queries/playlist_query.cc"
  "src/queries/artwork_query.cc"
  "src/queries/audios_from_query.cc"
  "src/queries/with_filters_query.cc"
  "src/queries/folder_query.cc"

  # Utils
  "src/utils/string_utils.cc"
)

# Apply Flutter plugin settings
apply_standard_settings(${PLUGIN_NAME})

# Additional plugin settings
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO
)

target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)

# Include directories
target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_include_directories(${PLUGIN_NAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  ${SQLITE3_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PLUGIN_NAME} PRIVATE
  flutter
  PkgConfig::GTK
  ${SQLITE3_LIBRARIES}
  nlohmann_json::nlohmann_json
  pthread
  stdc++fs
)

# Compiler flags for performance
target_compile_options(${PLUGIN_NAME} PRIVATE
  -O3
  -Wall
  -Wextra
)
