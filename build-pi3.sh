#!/bin/bash
set -e

# Optimized build script for Raspberry Pi 3B+ (Cortex-A53)
# Focuses on reducing CPU usage for real-time audio processing

# Check swap space (Pi 3B+ needs swap for compilation)
echo "Checking system resources..."
SWAP_SIZE=$(free -m | awk '/^Swap:/ {print $2}')
if [ "$SWAP_SIZE" -lt 1024 ]; then
    echo "WARNING: Low swap space detected (${SWAP_SIZE}MB)"
    echo "Pi 3B+ needs at least 1GB swap for compilation"
    echo "Current swap: ${SWAP_SIZE}MB"
    echo ""

    # Offer to create temporary swap file
    read -p "Create temporary 2GB swap file? (y/N) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Creating 2GB swap file (this may take a few minutes)..."
        sudo fallocate -l 2G /swapfile || sudo dd if=/dev/zero of=/swapfile bs=1M count=2048
        sudo chmod 600 /swapfile
        sudo mkswap /swapfile
        sudo swapon /swapfile
        echo "Swap file created and activated"
        echo "Note: This swap file will be removed after reboot"
        echo "To make it permanent, add '/swapfile none swap sw 0 0' to /etc/fstab"
    else
        echo ""
        echo "Continuing without additional swap..."
        echo "Build may fail due to insufficient memory"
    fi
fi

# Check for required dependencies
echo "Checking dependencies..."
MISSING_DEPS=()

if ! dpkg -s libasound2-dev &>/dev/null; then
    MISSING_DEPS+=("libasound2-dev")
fi

if ! dpkg -s libjack-jackd2-dev &>/dev/null; then
    MISSING_DEPS+=("libjack-jackd2-dev")
fi

if ! dpkg -s cmake &>/dev/null; then
    MISSING_DEPS+=("cmake")
fi

if ! dpkg -s build-essential &>/dev/null; then
    MISSING_DEPS+=("build-essential")
fi

if ! dpkg -s libsndfile1-dev &>/dev/null; then
    MISSING_DEPS+=("libsndfile1-dev")
fi

if ! dpkg -s libmpg123-dev &>/dev/null; then
    MISSING_DEPS+=("libmpg123-dev")
fi

if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
    echo "Missing dependencies: ${MISSING_DEPS[*]}"
    echo "Installing dependencies..."
    sudo apt-get update
    sudo apt-get install -y "${MISSING_DEPS[@]}"
    echo "Dependencies installed."
fi

# Parse arguments
BUILD_STATIC=false
if [[ "$1" == "--static" ]]; then
    BUILD_STATIC=true
    echo "=========================================="
    echo "  HoopiPi STATIC Build for Pi 3B+"
    echo "  WARNING: This will take 30-60+ minutes!"
    echo "=========================================="
else
    echo "=========================================="
    echo "  HoopiPi Fast Build for Pi 3B+"
    echo "  (Use --static for optimized build)"
    echo "=========================================="
fi

# Create build directory
mkdir -p build-pi3
cd build-pi3

# Configure with Pi 3B+ optimizations
if [ "$BUILD_STATIC" = true ]; then
    cmake .. \
      -DCMAKE_BUILD_TYPE=Release \
      -DWAVENET_FRAMES=128 \
      -DBUFFER_PADDING=8 \
      -DBUILD_STATIC_RTNEURAL=ON \
      -DWAVENET_MATH=FastMath \
      -DLSTM_MATH=FastMath

    echo ""
    echo "Configuration Summary:"
    echo "  - Build mode: STATIC (optimized, slow compile)"
    echo "  - Target CPU: native (auto-detected)"
    echo "  - WaveNet frame size: 128 (matches JACK buffer)"
    echo "  - Buffer padding: 8 (reduced for Pi 3B+)"
    echo "  - Static RTNeural: ON (faster runtime)"
    echo "  - Fast math: ON (both WaveNet and LSTM)"
    echo ""
else
    cmake .. \
      -DCMAKE_BUILD_TYPE=Release \
      -DWAVENET_FRAMES=128 \
      -DBUFFER_PADDING=8 \
      -DWAVENET_MATH=FastMath \
      -DLSTM_MATH=FastMath

    echo ""
    echo "Configuration Summary:"
    echo "  - Build mode: DYNAMIC (fast compile)"
    echo "  - Target CPU: native (auto-detected)"
    echo "  - WaveNet frame size: 128 (matches JACK buffer)"
    echo "  - Buffer padding: 8 (reduced for Pi 3B+)"
    echo "  - Static RTNeural: OFF (faster compile)"
    echo "  - Fast math: ON (both WaveNet and LSTM)"
    echo ""
fi

# Build with limited parallelism to avoid OOM on Pi 3B+ (only 1GB RAM)
# Use single-threaded build to stay within memory limits
echo "Building with 1 core (Pi 3B+ has limited RAM)..."
echo "Started at: $(date)"
echo "This will take 10-15 minutes..."
make -j1

echo ""
echo "=========================================="
echo "  Build Complete!"
echo "=========================================="
echo "Finished at: $(date)"
echo ""
echo "Binaries:"
echo "  - build-pi3/standalone/HoopiPi"
echo "  - build-pi3/api-server/HoopiPiAPI"
echo ""
echo "To install:"
echo "  sudo cp build-pi3/standalone/HoopiPi /usr/local/bin/"
echo "  sudo cp build-pi3/api-server/HoopiPiAPI /usr/local/bin/"
echo "  systemctl --user restart hoopi-engine hoopi-api"
echo ""
echo "For best performance, also increase JACK buffer:"
echo "  echo '/usr/bin/jackd -dalsa -dhw:0 -r48000 -p256 -n2' > ~/.jackdrc"
echo "  systemctl --user restart hoopi-jack"
echo ""
