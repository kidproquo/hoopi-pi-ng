#!/bin/bash
set -e

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Parse command-line arguments
AUTO_INCREMENT=true
SET_VERSION=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --version)
            SET_VERSION="$2"
            AUTO_INCREMENT=false
            shift 2
            ;;
        --major)
            BUMP_MAJOR=true
            AUTO_INCREMENT=false
            shift
            ;;
        --minor)
            BUMP_MINOR=true
            AUTO_INCREMENT=false
            shift
            ;;
        --patch)
            BUMP_PATCH=true
            AUTO_INCREMENT=false
            shift
            ;;
        --no-increment)
            AUTO_INCREMENT=false
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --version X.Y.Z    Set specific version"
            echo "  --major            Bump major version (X.0.0-1)"
            echo "  --minor            Bump minor version (x.Y.0-1)"
            echo "  --patch            Bump patch version (x.y.Z-1)"
            echo "  --no-increment     Build without incrementing version"
            echo "  --help             Show this help"
            echo ""
            echo "Default: Auto-increment build number (x.y.z-BUILD)"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Run with --help for usage"
            exit 1
            ;;
    esac
done

echo -e "${BLUE}======================================${NC}"
echo -e "${BLUE}   HoopiPi Debian Package Builder${NC}"
echo -e "${BLUE}======================================${NC}"

# Get current version from control file
CURRENT_VERSION=$(grep "^Version:" debian/DEBIAN/control | cut -d' ' -f2)

# Parse version components (format: MAJOR.MINOR.PATCH or MAJOR.MINOR.PATCH-BUILD)
if [[ $CURRENT_VERSION =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)(-([0-9]+))?$ ]]; then
    MAJOR="${BASH_REMATCH[1]}"
    MINOR="${BASH_REMATCH[2]}"
    PATCH="${BASH_REMATCH[3]}"
    BUILD="${BASH_REMATCH[5]:-0}"
else
    echo -e "${RED}Error: Invalid version format in control file: $CURRENT_VERSION${NC}"
    exit 1
fi

# Calculate new version based on mode
if [ -n "$SET_VERSION" ]; then
    # User specified exact version
    NEW_VERSION="$SET_VERSION"
    echo -e "${YELLOW}Setting version: ${CURRENT_VERSION} -> ${NEW_VERSION}${NC}"
elif [ "$BUMP_MAJOR" = true ]; then
    # Bump major version
    MAJOR=$((MAJOR + 1))
    MINOR=0
    PATCH=0
    BUILD=1
    NEW_VERSION="${MAJOR}.${MINOR}.${PATCH}-${BUILD}"
    echo -e "${YELLOW}Bumping major version: ${CURRENT_VERSION} -> ${NEW_VERSION}${NC}"
elif [ "$BUMP_MINOR" = true ]; then
    # Bump minor version
    MINOR=$((MINOR + 1))
    PATCH=0
    BUILD=1
    NEW_VERSION="${MAJOR}.${MINOR}.${PATCH}-${BUILD}"
    echo -e "${YELLOW}Bumping minor version: ${CURRENT_VERSION} -> ${NEW_VERSION}${NC}"
elif [ "$BUMP_PATCH" = true ]; then
    # Bump patch version
    PATCH=$((PATCH + 1))
    BUILD=1
    NEW_VERSION="${MAJOR}.${MINOR}.${PATCH}-${BUILD}"
    echo -e "${YELLOW}Bumping patch version: ${CURRENT_VERSION} -> ${NEW_VERSION}${NC}"
elif [ "$AUTO_INCREMENT" = true ]; then
    # Auto-increment build number
    BUILD=$((BUILD + 1))
    NEW_VERSION="${MAJOR}.${MINOR}.${PATCH}-${BUILD}"
    echo -e "${YELLOW}Auto-incrementing build: ${CURRENT_VERSION} -> ${NEW_VERSION}${NC}"
else
    # No increment
    NEW_VERSION="$CURRENT_VERSION"
    echo -e "${YELLOW}Building version: ${NEW_VERSION} (no increment)${NC}"
fi

# Update version in control file if changed
if [ "$NEW_VERSION" != "$CURRENT_VERSION" ]; then
    sed -i "s/^Version:.*/Version: ${NEW_VERSION}/" debian/DEBIAN/control
fi

VERSION="$NEW_VERSION"

# Detect architecture
ARCH=$(uname -m)
case "$ARCH" in
    aarch64|arm64)
        ARCH_NAME="arm64"
        ;;
    armv7l)
        ARCH_NAME="armhf"
        ;;
    x86_64)
        ARCH_NAME="amd64"
        ;;
    *)
        ARCH_NAME="$ARCH"
        ;;
esac

# Detect CPU model from /proc/cpuinfo
CPU_MODEL=""
if [ -f /proc/cpuinfo ]; then
    # Try to get CPU model from different fields
    CPU_INFO=$(grep -m1 -E "^(model name|Hardware|CPU part)" /proc/cpuinfo | head -1)

    if echo "$CPU_INFO" | grep -q "Cortex-A53"; then
        CPU_MODEL="cortex-a53"
    elif echo "$CPU_INFO" | grep -q "Cortex-A72"; then
        CPU_MODEL="cortex-a72"
    elif echo "$CPU_INFO" | grep -q "Cortex-A76"; then
        CPU_MODEL="cortex-a76"
    else
        # Try to detect from CPU part number
        CPU_PART=$(grep "CPU part" /proc/cpuinfo | head -1 | awk '{print $4}')
        case "$CPU_PART" in
            0xd03)
                CPU_MODEL="cortex-a53"
                ;;
            0xd08)
                CPU_MODEL="cortex-a72"
                ;;
            0xd0b)
                CPU_MODEL="cortex-a76"
                ;;
        esac
    fi
fi

# Check for -mcpu flag in build flags
MCPU_VALUE=""
if [ -f "build/standalone/CMakeFiles/HoopiPi.dir/flags.make" ]; then
    # Extract CXX_FLAGS from the flags.make file
    ACTUAL_CXX_FLAGS=$(grep "^CXX_FLAGS =" build/standalone/CMakeFiles/HoopiPi.dir/flags.make | cut -d'=' -f2-)

    # Check for -mcpu flag
    if echo "$ACTUAL_CXX_FLAGS" | grep -q "\-mcpu="; then
        MCPU_VALUE=$(echo "$ACTUAL_CXX_FLAGS" | grep -o "\-mcpu=[^ ]*" | cut -d'=' -f2-)
        echo -e "${BLUE}Detected -mcpu flag: ${MCPU_VALUE}${NC}"

        # If -mcpu=native, use the detected CPU model instead
        if [ "$MCPU_VALUE" = "native" ]; then
            if [ -n "$CPU_MODEL" ]; then
                echo -e "${YELLOW}Replacing -mcpu=native with detected CPU: ${CPU_MODEL}${NC}"
                MCPU_VALUE="$CPU_MODEL"
            else
                echo -e "${YELLOW}Warning: -mcpu=native used but could not detect actual CPU model${NC}"
            fi
        fi
    # Also check for -march flag as fallback
    elif echo "$ACTUAL_CXX_FLAGS" | grep -q "\-march="; then
        MARCH_VALUE=$(echo "$ACTUAL_CXX_FLAGS" | grep -o "\-march=[^ ]*" | cut -d'=' -f2-)
        echo -e "${BLUE}Detected -march flag: ${MARCH_VALUE}${NC}"

        # If -march=native, use the detected CPU model instead
        if [ "$MARCH_VALUE" = "native" ]; then
            if [ -n "$CPU_MODEL" ]; then
                echo -e "${YELLOW}Replacing -march=native with detected CPU: ${CPU_MODEL}${NC}"
                MCPU_VALUE="$CPU_MODEL"
            else
                echo -e "${YELLOW}Warning: -march=native used but could not detect actual CPU model${NC}"
                MCPU_VALUE="native"
            fi
        else
            MCPU_VALUE="$MARCH_VALUE"
        fi
    fi
fi

# Build package name with architecture and CPU model
# Prefer -mcpu/march flag value over detected CPU, fall back to detected if no flag
if [ -n "$MCPU_VALUE" ]; then
    PACKAGE_NAME="hoopi-pi_${VERSION}_${ARCH_NAME}_${MCPU_VALUE}"
    PACKAGE_CPU="$MCPU_VALUE"
elif [ -n "$CPU_MODEL" ]; then
    PACKAGE_NAME="hoopi-pi_${VERSION}_${ARCH_NAME}_${CPU_MODEL}"
    PACKAGE_CPU="$CPU_MODEL"
else
    PACKAGE_NAME="hoopi-pi_${VERSION}_${ARCH_NAME}"
    PACKAGE_CPU="unknown"
fi

echo -e "${BLUE}Building package: ${PACKAGE_NAME}.deb${NC}"
echo -e "${BLUE}Architecture: ${ARCH_NAME}, CPU: ${PACKAGE_CPU}${NC}"

# Clean previous build
echo -e "${BLUE}Cleaning previous build...${NC}"
# Save hoopi-check-jack if it exists
if [ -f "debian/usr/local/bin/hoopi-check-jack" ]; then
    cp debian/usr/local/bin/hoopi-check-jack /tmp/hoopi-check-jack.tmp
fi

rm -rf debian/usr/local/bin/*
rm -rf debian/usr/local/share
mkdir -p debian/usr/local/bin
mkdir -p debian/usr/local/share/hoopi-pi/web-ui
mkdir -p debian/usr/local/share/hoopi-pi/models
mkdir -p debian/usr/local/share/hoopi-pi/recordings

# Restore hoopi-check-jack
if [ -f "/tmp/hoopi-check-jack.tmp" ]; then
    cp /tmp/hoopi-check-jack.tmp debian/usr/local/bin/hoopi-check-jack
    rm /tmp/hoopi-check-jack.tmp
fi

# Check if binaries exist
if [ ! -f "build/standalone/HoopiPi" ]; then
    echo -e "${RED}Error: HoopiPi binary not found. Please run 'make' first.${NC}"
    exit 1
fi

if [ ! -f "build/api-server/HoopiPiAPI" ]; then
    echo -e "${RED}Error: HoopiPiAPI binary not found. Please run 'make' first.${NC}"
    exit 1
fi

# Copy binaries
echo -e "${BLUE}Copying binaries...${NC}"
cp build/standalone/HoopiPi debian/usr/local/bin/
cp build/api-server/HoopiPiAPI debian/usr/local/bin/

# Strip binaries to reduce size
echo -e "${BLUE}Stripping debug symbols...${NC}"
strip debian/usr/local/bin/HoopiPi
strip debian/usr/local/bin/HoopiPiAPI

# Copy helper script (it's already in the debian directory structure)
# Just make sure it exists
if [ ! -f "debian/usr/local/bin/hoopi-check-jack" ]; then
    echo -e "${RED}Warning: hoopi-check-jack helper script not found${NC}"
fi

# Copy web UI
echo -e "${BLUE}Copying web UI...${NC}"
if [ -d "build/web-ui" ]; then
    cp -r build/web-ui/* debian/usr/local/share/hoopi-pi/web-ui/
else
    echo -e "${RED}Warning: Web UI not found at build/web-ui${NC}"
fi

# Copy models directory (if exists)
if [ -d "build/models" ]; then
    echo -e "${BLUE}Copying NAM models...${NC}"
    cp -r build/models/* debian/usr/local/share/hoopi-pi/models/ 2>/dev/null || true
    MODEL_COUNT=$(find build/models -name "*.nam" | wc -l)
    echo -e "${GREEN}Copied ${MODEL_COUNT} NAM models${NC}"
fi

# Inject build metadata into systemd service
echo -e "${BLUE}Injecting build metadata into service files...${NC}"
BUILD_DATE=$(date -u +"%Y-%m-%d %H:%M:%S UTC")

# Extract build flags from CMake cache
BUILD_FLAGS="Unknown"
if [ -f "build/CMakeCache.txt" ]; then
    CMAKE_BUILD_TYPE=$(grep "^CMAKE_BUILD_TYPE:STRING=" build/CMakeCache.txt | cut -d'=' -f2-)
    CMAKE_CXX_FLAGS=$(grep "^CMAKE_CXX_FLAGS:STRING=" build/CMakeCache.txt | cut -d'=' -f2-)

    # Get Release/Debug-specific flags
    if [ "$CMAKE_BUILD_TYPE" = "Release" ]; then
        CMAKE_TYPE_FLAGS=$(grep "^CMAKE_CXX_FLAGS_RELEASE:STRING=" build/CMakeCache.txt | cut -d'=' -f2-)
    elif [ "$CMAKE_BUILD_TYPE" = "Debug" ]; then
        CMAKE_TYPE_FLAGS=$(grep "^CMAKE_CXX_FLAGS_DEBUG:STRING=" build/CMakeCache.txt | cut -d'=' -f2-)
    fi

    # Get BUILD_* options
    BUILD_STATIC_RTNEURAL=$(grep "^BUILD_STATIC_RTNEURAL:BOOL=" build/CMakeCache.txt | cut -d'=' -f2-)
    BUILD_NAMCORE=$(grep "^BUILD_NAMCORE:BOOL=" build/CMakeCache.txt | cut -d'=' -f2-)
    BUILD_UTILS=$(grep "^BUILD_UTILS:BOOL=" build/CMakeCache.txt | cut -d'=' -f2-)

    # Combine all flags
    BUILD_FLAGS="${CMAKE_BUILD_TYPE}"
    [ -n "$CMAKE_TYPE_FLAGS" ] && BUILD_FLAGS="${BUILD_FLAGS} ${CMAKE_TYPE_FLAGS}"
    [ -n "$CMAKE_CXX_FLAGS" ] && BUILD_FLAGS="${BUILD_FLAGS} ${CMAKE_CXX_FLAGS}"
    [ "$BUILD_STATIC_RTNEURAL" = "ON" ] && BUILD_FLAGS="${BUILD_FLAGS} -DBUILD_STATIC_RTNEURAL=ON"
    [ "$BUILD_NAMCORE" = "ON" ] && BUILD_FLAGS="${BUILD_FLAGS} -DBUILD_NAMCORE=ON"
    [ "$BUILD_UTILS" = "ON" ] && BUILD_FLAGS="${BUILD_FLAGS} -DBUILD_UTILS=ON"
fi

# Update hoopi-engine.service with build metadata
sed -i "s|X-HoopiPi-Version=.*|X-HoopiPi-Version=${VERSION}|g" debian/lib/systemd/system/hoopi-engine.service
sed -i "s|X-HoopiPi-PackageName=.*|X-HoopiPi-PackageName=${PACKAGE_NAME}.deb|g" debian/lib/systemd/system/hoopi-engine.service
sed -i "s|X-HoopiPi-BuildDate=.*|X-HoopiPi-BuildDate=${BUILD_DATE}|g" debian/lib/systemd/system/hoopi-engine.service
sed -i "s|X-HoopiPi-Architecture=.*|X-HoopiPi-Architecture=${ARCH_NAME}|g" debian/lib/systemd/system/hoopi-engine.service
sed -i "s|X-HoopiPi-CPU=.*|X-HoopiPi-CPU=${PACKAGE_CPU}|g" debian/lib/systemd/system/hoopi-engine.service
sed -i "s|X-HoopiPi-BuildFlags=.*|X-HoopiPi-BuildFlags=${BUILD_FLAGS}|g" debian/lib/systemd/system/hoopi-engine.service

# Set permissions
echo -e "${BLUE}Setting permissions...${NC}"
chmod 755 debian/DEBIAN/postinst
chmod 755 debian/DEBIAN/prerm
chmod 755 debian/usr/local/bin/HoopiPi
chmod 755 debian/usr/local/bin/HoopiPiAPI
chmod 755 debian/usr/local/bin/hoopi-check-jack
chmod 644 debian/etc/security/limits.d/99-hoopi-pi-audio.conf
chmod 644 debian/lib/systemd/system/*.service

# Build the package
# Use --root-owner-group to set correct ownership without needing sudo
echo -e "${BLUE}Building .deb package...${NC}"
dpkg-deb --root-owner-group --build debian "${PACKAGE_NAME}.deb"

# Show package info
echo -e "${GREEN}======================================${NC}"
echo -e "${GREEN}Package built successfully!${NC}"
echo -e "${GREEN}======================================${NC}"
echo -e "Package: ${PACKAGE_NAME}.deb"
echo -e "Size: $(du -h ${PACKAGE_NAME}.deb | cut -f1)"
echo -e ""
echo -e "Install with:"
echo -e "  ${BLUE}sudo apt-get install ./${PACKAGE_NAME}.deb${NC}"
echo -e ""
echo -e "Package contents:"
dpkg-deb --contents "${PACKAGE_NAME}.deb" | head -20
echo -e "..."
