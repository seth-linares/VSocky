#!/bin/bash
# scripts/compress-binary.sh - Compress vsocky binary with UPX (optional)
#
# This reduces binary size significantly but adds ~50ms to startup time
# Only use if binary size is more important than startup performance

set -e

BINARY="${1:-build-alpine/vsocky}"

if [ ! -f "$BINARY" ]; then
    echo "Error: Binary not found: $BINARY"
    echo "Usage: $0 [path/to/binary]"
    exit 1
fi

echo "Compressing $BINARY with UPX..."
echo "Original size: $(ls -lh "$BINARY" | awk '{print $5}')"

# Check if we're in Alpine container or host
if [ -f /etc/alpine-release ]; then
    # Inside Alpine container
    if ! command -v upx &> /dev/null; then
        echo "Installing UPX..."
        apk add --no-cache upx
    fi
else
    # On host system
    if ! command -v upx &> /dev/null; then
        echo "Error: UPX not installed"
        echo "Install with: sudo apt-get install upx-ucl"
        exit 1
    fi
fi

# Create backup
cp "$BINARY" "${BINARY}.uncompressed"

# Compress with best settings
upx --best --lzma "$BINARY"

echo ""
echo "Compression complete!"
echo "Original: $(ls -lh "${BINARY}.uncompressed" | awk '{print $5}')"
echo "Compressed: $(ls -lh "$BINARY" | awk '{print $5}')"
echo ""
echo "Note: This adds ~50ms to startup time"
echo "Backup saved as: ${BINARY}.uncompressed"