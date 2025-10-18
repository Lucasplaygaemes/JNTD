#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

# --- Helper Functions ---
info() {
    echo -e "[1;34m[INFO][0m $1"
}

error() {
    echo -e "[1;31m[ERROR][0m $1" >&2
    exit 1
}

# --- Dependency Installation ---
install_dependencies() {
    info "Checking and installing system dependencies..."
    
    if ! command -v sudo &> /dev/null; then
        error "'sudo' command not found. Please install sudo or run this script as root."
    fi
    if ! command -v git &> /dev/null; then
        error "'git' command not found. Please install git."
    fi

    if command -v apt-get &> /dev/null; then
        info "Detected APT package manager (Debian/Ubuntu)."
        sudo apt-get update
        sudo apt-get install -y build-essential libncursesw5-dev libjansson-dev libcurl4-openssl-dev libssl-dev git cmake libutil-dev
    elif command -v dnf &> /dev/null; then
        info "Detected DNF package manager (Fedora)."
        sudo dnf install -y gcc ncurses-devel jansson-devel libcurl-devel openssl-devel git cmake glibc-devel
    elif command -v pacman &> /dev/null; then
        info "Detected Pacman package manager (Arch Linux)."
        sudo pacman -Syu --noconfirm base-devel ncurses jansson libcurl-openssl openssl git cmake
    else
        error "Unsupported package manager. Please install dependencies manually."
    fi
    info "System dependencies installed successfully."
}

# --- Compile Project ---
compile_project() {
    info "Compiling jntd..."
    make clean
    make
    info "Compilation successful."
}

# --- Install Binaries ---
install_binaries() {
    info "Installing jntd binary to /usr/local/bin/..."
    if [ ! -f "jntd" ]; then
        error "Binary not found. Compilation may have failed."
    fi

    sudo cp jntd /usr/local/bin/jntd
    
    info "Installation complete!"
    info "You can now run 'jntd' from anywhere in your terminal."
}

# --- Main Execution ---
main() {
    install_dependencies
    compile_project
    install_binaries
}

main "$@"