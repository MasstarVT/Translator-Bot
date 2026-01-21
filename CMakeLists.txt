cmake_minimum_required(VERSION 3.15)
project(discord-bot VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)

# Check for opus and sodium (optional, for voice support)
pkg_check_modules(OPUS opus)
pkg_check_modules(SODIUM libsodium)

# Add DPP (Discord C++ library)
include(FetchContent)

FetchContent_Declare(
    dpp
    GIT_REPOSITORY https://github.com/brainboxdotcc/DPP.git
    GIT_TAG        v10.0.30
)
FetchContent_MakeAvailable(dpp)

# Add nlohmann/json library
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
    src/bot.cpp
    src/config.cpp
    src/database.cpp
    src/utils/common.cpp
    src/utils/string_utils.cpp
    src/utils/curl_helper.cpp
    src/utils/thread_pool.cpp
    src/modules/translation.cpp
    src/modules/moderation.cpp
    src/modules/leveling.cpp
    src/modules/custom_commands.cpp
    src/modules/welcome.cpp
    src/modules/music.cpp
    src/modules/reaction_roles.cpp
    src/modules/logging.cpp
    src/modules/notifications.cpp
)

# Create executable
add_executable(discord-bot ${SOURCES})

# Link libraries
target_link_libraries(discord-bot
    PRIVATE
    dpp
    nlohmann_json::nlohmann_json
    CURL::libcurl
    SQLite::SQLite3
    pthread
)

# Add opus and sodium if available (for voice support)
if(OPUS_FOUND)
    target_link_libraries(discord-bot PRIVATE ${OPUS_LIBRARIES})
    target_include_directories(discord-bot PRIVATE ${OPUS_INCLUDE_DIRS})
    target_compile_definitions(discord-bot PRIVATE HAVE_OPUS)
endif()

if(SODIUM_FOUND)
    target_link_libraries(discord-bot PRIVATE ${SODIUM_LIBRARIES})
    target_include_directories(discord-bot PRIVATE ${SODIUM_INCLUDE_DIRS})
    target_compile_definitions(discord-bot PRIVATE HAVE_SODIUM)
endif()

# Set output directory
set_target_properties(discord-bot PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(discord-bot PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Debug/Release configurations
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(discord-bot PRIVATE DEBUG)
endif()

# Install target
install(TARGETS discord-bot DESTINATION bin)
install(DIRECTORY data/ DESTINATION share/discord-bot/data)
