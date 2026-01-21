cmake_minimum_required(VERSION 3.15)
project(discord-translation-bot)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(CURL REQUIRED)

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

# Create executable
add_executable(discord-bot bot.cpp)

# Link libraries
target_link_libraries(discord-bot
    PRIVATE
    dpp
    nlohmann_json::nlohmann_json
    CURL::libcurl
    pthread
)

# Set output directory
set_target_properties(discord-bot PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
