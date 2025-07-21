# VSocky - Firecracker VM Code Execution Service

A lightweight, efficient VSock server for executing code safely within Firecracker microVMs.

## Quick Start

### Prerequisites

- CMake 3.28+
- C++23 compatible compiler (GCC 14+)
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

### Binary Size Optimization

Current binary sizes:
- Local build: ~200KB (dynamic linking)
- Alpine build: ~784KB (static, with simdjson)

Note: simdjson adds ~140KB. Consider alternatives if size is critical.