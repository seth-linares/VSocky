# VSocky Makefile - Quick commands for development
.PHONY: all build build-alpine test clean shell help analyze compress size-optimize

# Default target
all: build

# Build for local development
build:
	@./scripts/build-local.sh

# Build static Alpine binary
build-alpine:
	@./scripts/build-alpine.sh

# Run tests
test:
	@./scripts/dev.sh test local

test-alpine:
	@./scripts/dev.sh test alpine

# Clean all builds
clean:
	@./scripts/dev.sh clean
	@rm -rf build-alpine-no-json build-alpine-debug

# Development shells
shell:
	@./scripts/dev-shell.sh ubuntu

shell-alpine:
	@./scripts/dev-shell.sh alpine

# Check environment
check:
	@./scripts/check-env.sh

# Show binary sizes
size:
	@./scripts/dev.sh size

# Analyze binary size (what's making it large)
analyze:
	@./scripts/analyze-binary.sh


# Show help
help:
	@echo "VSocky Development Commands:"
	@echo "  make build         - Build for local development (Ubuntu/WSL2)"
	@echo "  make build-alpine  - Build static binary for Alpine"
	@echo "  make test          - Run local tests"
	@echo "  make test-alpine   - Run Alpine tests"
	@echo "  make clean         - Clean all build directories"
	@echo "  make shell         - Start Ubuntu dev shell"
	@echo "  make shell-alpine  - Start Alpine dev shell"
	@echo "  make check         - Check development environment"
	@echo "  make size          - Show binary sizes"
	@echo "  make analyze       - Analyze what's making binary large"
	@echo "  make help          - Show this help"