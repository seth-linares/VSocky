#!/bin/bash
# Interactive development shell

ENV="${1:-ubuntu}"

case "$ENV" in
    alpine)
        echo "Starting Alpine development shell..."
        docker run --rm -it \
            -v "$(pwd):/workspace" \
            -w /workspace \
            -e TERM=xterm-256color \
            vsocky-alpine-build \
            /bin/sh
        ;;
    ubuntu)
        echo "Starting Ubuntu development shell..."
        docker run --rm -it \
            -v "$(pwd):/workspace" \
            -w /workspace \
            -e TERM=xterm-256color \
            --cap-add=SYS_PTRACE \
            --security-opt seccomp=unconfined \
            vsocky-ubuntu-build \
            /bin/bash
        ;;
    *)
        echo "Usage: $0 [alpine|ubuntu]"
        echo "  alpine - Alpine Linux development environment"
        echo "  ubuntu - Ubuntu development environment (default)"
        exit 1
        ;;
esac