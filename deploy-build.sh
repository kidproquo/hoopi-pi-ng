#!/bin/bash
# HoopiPi Deployment Build Script
# Builds both the C++ backend and React frontend for production deployment

set -e  # Exit on error

echo "======================================"
echo "HoopiPi Production Build"
echo "======================================"
echo ""

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Check if node/npm is installed
if ! command -v npm &> /dev/null; then
    echo "Error: npm not found. Please install Node.js and npm first."
    exit 1
fi

# Build frontend
echo "Step 1: Building Web UI..."
echo "--------------------------------------"
cd web-ui
npm install
npm run build
cd ..
echo "✓ Web UI built successfully"
echo ""

# Build backend
echo "Step 2: Building C++ Backend..."
echo "--------------------------------------"
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4 HoopiPi HoopiPiAPI
cd ..
echo "✓ Backend built successfully"
echo ""

# Verify build outputs
echo "Step 3: Verifying build outputs..."
echo "--------------------------------------"
if [ ! -d "build/web-ui" ]; then
    echo "Error: Web UI build directory not found!"
    exit 1
fi

if [ ! -f "build/standalone/HoopiPi" ]; then
    echo "Error: HoopiPi engine executable not found!"
    exit 1
fi

if [ ! -f "build/api-server/HoopiPiAPI" ]; then
    echo "Error: API server executable not found!"
    exit 1
fi

echo "✓ Web UI: build/web-ui/"
echo "✓ HoopiPi Engine: build/standalone/HoopiPi"
echo "✓ API Server: build/api-server/HoopiPiAPI"
echo ""

echo "======================================"
echo "Build Complete!"
echo "======================================"
echo ""
echo "To run manually:"
echo ""
echo "  Terminal 1 - Start HoopiPi Engine:"
echo "    cd build/standalone"
echo "    ./HoopiPi"
echo ""
echo "  Terminal 2 - Start Web API Server:"
echo "    cd build/api-server"
echo "    ./HoopiPiAPI"
echo ""
echo "Or install as system services:"
echo "  sudo ./deploy-service.sh install"
echo ""
echo "The web interface will be available at:"
echo "  http://localhost:11995"
echo ""
