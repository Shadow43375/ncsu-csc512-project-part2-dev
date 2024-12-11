#!/bin/sh -eu

# Check if lld is installed
if ! command -v lld >/dev/null 2>&1; then
    echo "lld is not installed. Please install lld and try again."
    exit 1
fi

# Determine the architecture
ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ] || [ "$ARCH" = "aarch64" ]; then
    TARGETS="AArch64"
    processors=$(sysctl -n hw.physicalcpu)
else
    TARGETS="X86"
    processors=$(nproc)
fi

llvm_source_dir="../../llvm-source"
sourcedir="../../llvm-source/llvm"
buildir="../../llvm-build"
installdir="../../llvm-install"

# Check if LLVM version 17 is installed
if command -v llvm-config >/dev/null 2>&1; then
    installed_version=$(llvm-config --version | cut -d'.' -f1)
    if [ "$installed_version" != "17" ]; then
        echo -e "LLVM version 17 is not installed. Please install it to $llvm_source_dir and try again.\n\n"

        echo -e "The installation command is as follows:\n"
        echo "git clone --branch release/17.x --depth 1 https://github.com/llvm/llvm-project.git "$llvm_source_dir""
    fi
fi

# # Remove source directory if needed
# rm -rf "$llvm_source_dir"

# copy the modified llvm source code to the source directory
cp -r ../llvm-files/llvm-source/* "$llvm_source_dir"

# Clean build directory if needed
# rm -rf "$buildir"/*

echo "=== Configuring LLVM ==="
cmake -G Ninja -S "$sourcedir" \
      -B "$buildir" \
      -DLLVM_USE_LINKER=lld \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_INSTALL_PREFIX="$installdir" \
      -DLLVM_TARGETS_TO_BUILD="$TARGETS" \
      -DLLVM_ENABLE_PROJECTS="clang" \
      -DLLVM_BUILD_SHARED_LIBS=ON \
      -DLLVM_BUILD_TOOLS=ON \
      -DLLVM_INCLUDE_UTILS=ON \
      -DLLVM_ENABLE_ASSERTIONS=ON

processors=$(awk "BEGIN {print int(1.5 * $processors)}")

echo "=== Building LLVM ==="
ninja -C "$buildir" -j "$processors"

echo "=== Installing LLVM ==="
ninja -C "$buildir" -j "$processors" install

# Get absolute path of installdir
installdir="$(readlink -f "$installdir")"

# Export the bin directory to PATH
echo "=== Updating PATH ==="
if [ "$(id -u)" -eq 0 ] && [ -n "$SUDO_USER" ]; then
    USER_HOME=$(eval echo "~$SUDO_USER")
    if ! grep -q "$installdir/bin" "$USER_HOME/.bashrc"; then
        sudo -u "$SUDO_USER" bash -c "echo 'export PATH=\"$installdir/bin:\$PATH\"' >> \"$USER_HOME/.bashrc\""
        echo "Added LLVM binary path to $USER_HOME/.bashrc"
    fi
    echo "Please run 'source $USER_HOME/.bashrc' to update your PATH"
else
    if ! grep -q "$installdir/bin" ~/.bashrc; then
        echo 'export PATH="'$installdir'/bin:$PATH"' >> ~/.bashrc
        echo "Added LLVM binary path to ~/.bashrc"
    fi
    echo "Please run 'source ~/.bashrc' to update your PATH"
fi

echo "=== Build Complete ==="
echo "LLVM has been installed to: "$installdir""