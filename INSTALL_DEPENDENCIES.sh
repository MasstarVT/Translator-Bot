#!/bin/bash

# Dependency installation script for C++ Discord Bot

set -e

echo "========================================="
echo "Installing C++ Bot Dependencies"
echo "========================================="
echo ""

# Detect OS
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
else
    echo "Cannot detect OS"
    exit 1
fi

echo "Detected OS: $OS"
echo ""

case $OS in
    ubuntu|debian)
        echo "Installing dependencies for Ubuntu/Debian..."
        sudo apt update
        sudo apt install -y \
            build-essential \
            cmake \
            git \
            libcurl4-openssl-dev \
            libssl-dev \
            pkg-config
        ;;

    fedora|rhel|centos)
        echo "Installing dependencies for Fedora/RHEL/CentOS..."
        sudo dnf install -y \
            gcc-c++ \
            cmake \
            git \
            libcurl-devel \
            openssl-devel \
            pkg-config
        ;;

    arch|manjaro)
        echo "Installing dependencies for Arch Linux..."
        sudo pacman -S --noconfirm \
            base-devel \
            cmake \
            git \
            curl \
            openssl \
            pkg-config
        ;;

    *)
        echo "Unsupported OS: $OS"
        echo "Please install manually:"
        echo "  - CMake 3.15+"
        echo "  - C++ compiler (g++ or clang++)"
        echo "  - libcurl development libraries"
        echo "  - OpenSSL development libraries"
        exit 1
        ;;
esac

echo ""
echo "========================================="
echo "Dependencies installed successfully!"
echo "========================================="
echo ""
echo "You can now build the bot with:"
echo "  ./build.sh"
echo ""
