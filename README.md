# VSocky - Firecracker VM Code Execution Service

A lightweight, efficient VSock server for executing code safely within Firecracker microVMs.

## Quick Start

### Prerequisites

- CMake 3.28+
- C++23 compatible compiler (GCC 13+)
- Docker (for Alpine builds)

### Setup

```bash
# Make scripts executable
chmod +x scripts/*.sh

# Run initial setup
./scripts/setup.sh

# Build locally (Ubuntu/WSL2)
./scripts/build-local.sh

# Build for Alpine Linux
./scripts/build-alpine.sh
```

### Development Commands

```bash
# Using Make (recommended)
make build          # Build for local development
make build-alpine   # Build static Alpine binary
make size-optimize  # Build with maximum size optimization
make test           # Run tests
make analyze        # Analyze binary size composition
make compress       # Compress binary with UPX (optional)
make check          # Check development environment

# Using dev.sh directly
./scripts/dev.sh build alpine    # Build Alpine binary
./scripts/dev.sh test local      # Test local build
./scripts/dev.sh shell alpine    # Start Alpine dev shell
./scripts/dev.sh analyze         # Analyze binary size
./scripts/dev.sh compress alpine # Compress Alpine binary
```

### Binary Size Optimization

Current binary sizes:
- Local build: ~200KB (dynamic linking)
- Alpine build: ~784KB (static, with simdjson)

To reduce Alpine binary size:
1. `make size-optimize` - Build with -Os flag (~650KB)
2. `make analyze` - See what's taking space
3. `make compress` - Use UPX compression (~250KB)

Note: simdjson adds ~140KB. Consider alternatives if size is critical.

### Project Status

See [TODO.md](TODO.md) for implementation roadmap and progress.