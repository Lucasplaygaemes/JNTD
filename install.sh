#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

# --- Variables ---
VTERM_REPO_URL="https://github.com/TragicWarrior/libvterm.git"

# --- Helper Functions ---
info() {
    echo -e "\033[1;34m[INFO]\033[0m $1"
}

error() {
    echo -e "\033[1;31m[ERROR]\033[0m $1" >&2
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

# --- Install libvterm from Source ---
install_libvterm_from_source() {
    info "Checking for libvterm installation..."
    if [ -f "/usr/local/lib/libvterm.so" ]; then
        info "libvterm already seems to be installed. Skipping."
        return
    fi
    
    info "Cloning and installing libvterm from $VTERM_REPO_URL..."
    TMP_DIR=$(mktemp -d)
    git clone "$VTERM_REPO_URL" "$TMP_DIR"
    cd "$TMP_DIR"
    
    info "Configuring project with CMake..."
    cmake .
    
    info "Compiling libvterm..."
    make
    
    info "Installing libvterm on the system..."
    sudo make install
    
    cd - # Return to the original directory
    rm -rf "$TMP_DIR"
    
    info "Updating the system's library cache..."
    sudo ldconfig

    info "libvterm installed successfully."
}

# --- Compile Project ---
compile_project() {
    info "Compiling a2..."
    make clean
    make
    info "Compilation successful."
}

# --- Install Binaries ---
install_binaries() {
    info "Installing a2 and jntd binaries to /usr/local/bin/..."
    if [ ! -f "a2" ] || [ ! -f "jntd" ]; then
        error "Binaries not found. Compilation may have failed."
    fi

    sudo cp a2 /usr/local/bin/a2
    sudo cp jntd /usr/local/bin/jntd
    
    info "Installation complete!"
    info "You can now run 'a2' and 'jntd' from anywhere in your terminal."
}

# --- Main Execution ---
main() {
    install_dependencies
    install_libvterm_from_source
    compile_project
    install_binaries
}

main "$@"
