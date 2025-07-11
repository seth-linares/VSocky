#!/bin/bash

# ========================================================================
# HOW TO REMOVE IMAGES!!!
# ========================================================================
# docker rmi vsocky-alpine-build vsocky-ubuntu-build vsocky-runtime 2>/dev/null || true




set -e

echo "Setting up vsocky development environment..."

# Create directories
mkdir -p third_party
mkdir -p build
mkdir -p build-alpine
mkdir -p build-local

# Check for required tools
echo "Checking dependencies..."
MISSING_DEPS=()
for tool in cmake g++ git docker; do
    if ! command -v $tool &> /dev/null; then
        MISSING_DEPS+=($tool)
    fi
done

if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo "Error: Missing required tools: ${MISSING_DEPS[*]}"
    echo ""
    echo "Please install missing dependencies:"
    echo "  Ubuntu/Debian: sudo apt-get install ${MISSING_DEPS[*]}"
    echo "  Alpine: sudo apk add ${MISSING_DEPS[*]}"
    exit 1
fi

echo "All dependencies found!"

# Check Docker daemon
if ! docker info &> /dev/null; then
    echo "Error: Docker daemon is not running or you don't have permission"
    echo "Try: sudo systemctl start docker"
    echo "Or add your user to docker group: sudo usermod -aG docker $USER"
    exit 1
fi

# Clean up any existing images with our tags to ensure fresh builds
echo "Cleaning up old Docker images..."
docker rmi vsocky-alpine-build vsocky-ubuntu-build 2>/dev/null || true

# Build docker images
echo "Building Docker images..."
echo "Building Alpine build environment..."
docker build -t vsocky-alpine-build -f docker/Dockerfile.alpine-build docker/

echo "Building Ubuntu build environment..."
docker build -t vsocky-ubuntu-build -f docker/Dockerfile.ubuntu-build docker/

# Optional: Build runtime image if the Dockerfile exists and user wants it
if [ -f "docker/Dockerfile.vsocky-runtime" ] && [ "$BUILD_RUNTIME" = "yes" ]; then
    echo "Building runtime image..."
    docker build -t vsocky-runtime -f docker/Dockerfile.vsocky-runtime .
else
    echo "Skipping runtime image (not needed for development)"
fi

echo ""
echo "Setup complete!"
echo ""
echo "Available build commands:"
echo "  ./scripts/build-local.sh    - Build for local development (Ubuntu/WSL2)"
echo "  ./scripts/build-alpine.sh   - Build static binary for Alpine Linux"
echo ""
echo "Docker images created:"
docker images | grep vsocky-