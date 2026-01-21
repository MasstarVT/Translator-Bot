#!/bin/bash

# Build script for Discord Translation Bot (C++)

set -e

echo "==================================="
echo "Discord Translation Bot - C++ Build"
echo "==================================="
echo ""

# Check for required tools
echo "Checking for required tools..."

if ! command -v cmake &> /dev/null; then
    echo "Error: CMake is not installed"
    echo "Install with: sudo apt install cmake"
    exit 1
fi

if ! command -v g++ &> /dev/null; then
    echo "Error: g++ is not installed"
    echo "Install with: sudo apt install build-essential"
    exit 1
fi

echo "✓ CMake found: $(cmake --version | head -n1)"
echo "✓ g++ found: $(g++ --version | head -n1)"
echo ""

# Check for libcurl
if ! pkg-config --exists libcurl; then
    echo "Error: libcurl development libraries not found"
    echo "Install with: sudo apt install libcurl4-openssl-dev"
    exit 1
fi

echo "✓ libcurl found"
echo ""

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Configure
echo "Configuring project..."
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
echo ""
echo "Building project..."
cmake --build . -j$(nproc)

echo ""
echo "==================================="
echo "Build completed successfully!"
echo "==================================="
echo ""
echo "Executable location: build/bin/discord-bot"
echo ""
echo "To run the bot:"
echo "  cd .."
echo "  ./build/bin/discord-bot"
echo ""
