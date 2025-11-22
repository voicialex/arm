#!/bin/bash

# =============================================================================
# Arm Simulator Standalone Build Script
# =============================================================================

set -e

# Paths
ARM_DIR=$(cd "$(dirname "$0")" && pwd)
WORKSPACE_DIR=$(cd "$ARM_DIR/.." && pwd)
OUTPUT_DIR="$WORKSPACE_DIR/output"
PACKAGES_DIR="$OUTPUT_DIR/packages"
THIRDPARTY_DIR="$ARM_DIR/thirdparty"
BUILD_DIR="$ARM_DIR/build"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}🚀 Starting Arm Simulator Standalone Build...${NC}"

# 1. Check for runtime package
echo -e "${BLUE}🔍 Checking for runtime package...${NC}"
# Check in arm dir and thirdparty dir (standalone mode)
RUNTIME_DEB=$(find "$ARM_DIR" -maxdepth 2 -name "perception-app-*-x86_64.deb" 2>/dev/null | head -n 1)

# If not found, check in output packages (dev mode)
if [ -z "$RUNTIME_DEB" ]; then
    RUNTIME_DEB=$(find "$PACKAGES_DIR" -name "perception-app-*-x86_64.deb" 2>/dev/null | head -n 1)
fi

if [ -z "$RUNTIME_DEB" ]; then
    echo -e "${RED}❌ Runtime package not found in $ARM_DIR or $PACKAGES_DIR${NC}"
    echo -e "${YELLOW}Please build the runtime package first: ./build.sh -p${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Found runtime package: $RUNTIME_DEB${NC}"

# 2. Extract deb package to thirdparty
echo -e "${BLUE}📦 Extracting runtime package to $THIRDPARTY_DIR...${NC}"

# Create thirdparty dir if not exists
mkdir -p "$THIRDPARTY_DIR"

# Clean previous extraction (usr directory), but keep the deb file if it's in thirdparty
if [ -d "$THIRDPARTY_DIR/usr" ]; then
    rm -rf "$THIRDPARTY_DIR/usr"
fi

dpkg -x "$RUNTIME_DEB" "$THIRDPARTY_DIR"

if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Failed to extract package${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Package extracted${NC}"

# 3. Configure CMake
echo -e "${BLUE}⚙️  Configuring CMake...${NC}"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Determine install prefix from extracted deb
# Usually it's usr/local or usr
if [ -d "$THIRDPARTY_DIR/usr/local" ]; then
    PREFIX_PATH="$THIRDPARTY_DIR/usr/local"
elif [ -d "$THIRDPARTY_DIR/usr" ]; then
    PREFIX_PATH="$THIRDPARTY_DIR/usr"
else
    PREFIX_PATH="$THIRDPARTY_DIR"
fi

echo -e "${BLUE}ℹ️  Using prefix path: $PREFIX_PATH${NC}"

cmake .. \
    -DCMAKE_PREFIX_PATH="$PREFIX_PATH" \
    -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo -e "${RED}❌ CMake configuration failed${NC}"
    exit 1
fi

echo -e "${GREEN}✓ CMake configured${NC}"

# 4. Build
echo -e "${BLUE}🔨 Building...${NC}"
cmake --build . --parallel $(nproc)

if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Build failed${NC}"
    exit 1
fi

echo -e "${GREEN}🎉 Build successful!${NC}"
echo -e "Executable: $BUILD_DIR/arm"
