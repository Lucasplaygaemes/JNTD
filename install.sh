#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

# --- Helper Functions ---
info() {
    echo "[INFO] $1"
}

error() {
    echo "[ERROR] $1" >&2
    exit 1
}

# --- Dependency Installation ---
install_dependencies() {
    info "Checking and installing dependencies..."
    
    if ! command -v sudo &> /dev/null; then
        error "sudo command not found. Please install sudo or run this script as root."
    fi

    if command -v apt-get &> /dev/null; then
        info "Detected APT package manager."
        sudo apt-get update
        sudo apt-get install -y build-essential libncursesw5-dev libjansson-dev libcurl4-openssl-dev libssl-dev
    elif command -v dnf &> /dev/null; then
        info "Detected DNF package manager."
        sudo dnf install -y gcc ncurses-devel jansson-devel libcurl-devel openssl-devel
    elif command -v pacman &> /dev/null; then
        info "Detected Pacman package manager."
        sudo pacman -Syu --noconfirm base-devel ncurses jansson libcurl-openssl openssl
    else
        error "Unsupported package manager. Please install dependencies manually: make, gcc, libncursesw, libjansson, libcurl, openssl"
    fi
    info "Dependencies installed successfully."
}

# --- Compilation ---
compile_project() {
    info "Compiling the project..."
    make clean
    make
    info "Compilation successful."
}

# --- Installation (Symlink Method) ---
install_binaries() {
    info "Installing via symlinks (developer mode)..."
    if [ ! -f "a2" ] || [ ! -f "jntd" ]; then
        error "Compiled binaries not found. Compilation might have failed."
    fi

    # Get the absolute path to the current directory
    PROJECT_DIR=$(pwd)

    info "Creating symlinks in /usr/local/bin/..."
    sudo ln -sf "$PROJECT_DIR/a2" /usr/local/bin/a2
    sudo ln -sf "$PROJECT_DIR/jntd" /usr/local/bin/jntd
    
    info "Installation complete!"
    info "You can now run 'a2' and 'jntd' from anywhere."
    info "Changes to the code will be reflected after running 'make'."
}

# --- Main Execution ---
main() {
    install_dependencies
    compile_project
    install_binaries
}

main "$@"