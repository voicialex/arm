#!/bin/bash

# =============================================================================
# Arm Test Runner Script
# =============================================================================

set -e

# Paths
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ARM_BUILD_DIR="$SCRIPT_DIR/build"
THIRDPARTY_DIR="$SCRIPT_DIR/thirdparty"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# Check dependencies
check_dependencies() {
    if [ ! -f "$ARM_BUILD_DIR/arm" ]; then
        echo -e "${RED}Error: Arm executable not found at $ARM_BUILD_DIR/arm${NC}"
        echo -e "${YELLOW}Please build the arm project first: ./build.sh${NC}"
        exit 1
    fi

    # Find installation directory from thirdparty
    if [ -d "$THIRDPARTY_DIR/usr/local/bin" ]; then
        INSTALL_BIN_DIR="$THIRDPARTY_DIR/usr/local/bin"
        INSTALL_LIB_DIR="$THIRDPARTY_DIR/usr/local/lib"
    elif [ -d "$THIRDPARTY_DIR/usr/bin" ]; then
        INSTALL_BIN_DIR="$THIRDPARTY_DIR/usr/bin"
        INSTALL_LIB_DIR="$THIRDPARTY_DIR/usr/lib"
    else
        echo -e "${RED}Error: Could not find bin directory in $THIRDPARTY_DIR${NC}"
        exit 1
    fi

    if [ ! -f "$INSTALL_BIN_DIR/vision" ] || [ ! -f "$INSTALL_BIN_DIR/central" ]; then
        echo -e "${RED}Error: vision or central executable not found in $INSTALL_BIN_DIR${NC}"
        exit 1
    fi
}

# Kill existing processes
kill_existing() {
    echo -e "${BLUE}🔄 Stopping existing services...${NC}"
    pkill -f "vision" || true
    pkill -f "central" || true
    sleep 1
}

# Start processes in background
start_background_services() {
    echo -e "${BLUE}🚀 Starting services in background...${NC}"
    
    # Add both lib and lib/thirdparty to LD_LIBRARY_PATH
    export LD_LIBRARY_PATH="$INSTALL_LIB_DIR:$INSTALL_LIB_DIR/thirdparty:$INSTALL_LIB_DIR/thirdparty/externals:$LD_LIBRARY_PATH"
    
    # Set CONFIG_DIR
    local PREFIX_DIR=$(dirname "$INSTALL_BIN_DIR")
    export CONFIG_DIR="$PREFIX_DIR/share/perception_app/config"
    echo -e "  CONFIG_DIR set to: $CONFIG_DIR"
    
    cd "$INSTALL_BIN_DIR"
    
    nohup ./vision > /tmp/vision.log 2>&1 &
    echo -e "  Started Vision (PID: $!) - Logs: /tmp/vision.log"
    
    nohup ./central > /tmp/central.log 2>&1 &
    echo -e "  Started Central (PID: $!) - Logs: /tmp/central.log"
}

main() {
    echo -e "${BLUE}🔍 Checking dependencies...${NC}"
    check_dependencies
    
    echo -e "${GREEN}✓ Dependencies found.${NC}"
    echo -e "  Arm: $ARM_BUILD_DIR/arm"
    echo -e "  Vision: $INSTALL_BIN_DIR/vision"
    echo -e "  Central: $INSTALL_BIN_DIR/central"
    
    kill_existing
    start_background_services

    echo -e "${GREEN}✓ Services started successfully.${NC}"
    echo -e "${YELLOW}💡 Suggestion: Open web_ui/index.html to view terminal output/status.${NC}"
}

main "$@"
