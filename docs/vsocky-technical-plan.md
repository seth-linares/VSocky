# VSocky Technical Implementation Plan

## Project Structure

```
vsocky/
├── src/
│   ├── main.cpp                    # Entry point, minimal setup
│   ├── vsocket/
│   │   ├── server.hpp              # VSocket server abstraction
│   │   ├── server.cpp              # Non-blocking server implementation
│   │   ├── connection.hpp          # Single connection handler
│   │   └── connection.cpp          # Message framing, I/O operations
│   ├── protocol/
│   │   ├── message.hpp             # Message types and structures
│   │   ├── parser.hpp              # JSON parsing interface
│   │   ├── parser.cpp              # simdjson integration
│   │   ├── serializer.hpp          # Response building
│   │   └── serializer.cpp          # Efficient JSON output
│   ├── executor/
│   │   ├── executor.hpp            # Base execution interface
│   │   ├── executor.cpp            # Common execution logic
│   │   ├── languages/
│   │   │   ├── python.cpp          # Python-specific execution
│   │   │   ├── typescript.cpp      # TypeScript compilation & execution
│   │   │   ├── csharp.cpp          # C# compilation & execution
│   │   │   ├── cpp.cpp             # C++ compilation & execution
│   │   │   └── rust.cpp            # Rust compilation & execution
│   │   ├── process.hpp             # Process management
│   │   ├── process.cpp             # Fork, exec, wait logic
│   │   ├── sandbox.hpp             # Sandboxing utilities
│   │   └── sandbox.cpp             # Filesystem, permission setup
│   ├── monitor/
│   │   ├── resource_monitor.hpp    # Resource tracking interface
│   │   ├── resource_monitor.cpp    # /proc parsing, limits
│   │   ├── metrics.hpp             # Metrics collection
│   │   └── metrics.cpp             # Efficient metric storage
│   ├── utils/
│   │   ├── buffer_pool.hpp         # Pre-allocated buffer management
│   │   ├── buffer_pool.cpp         # Zero-copy buffer operations
│   │   ├── base64.hpp              # Fast base64 encoding/decoding
│   │   ├── base64.cpp              # SIMD-optimized implementation
│   │   ├── error.hpp               # Error handling utilities
│   │   └── logger.hpp              # Compile-time logging
│   └── config.hpp                  # Compile-time configuration
├── include/                        # Public headers (if needed)
├── tests/
│   ├── unit/                       # Unit tests for each component
│   ├── integration/                # Full execution tests
│   └── benchmarks/                 # Performance benchmarks
└── CMakeLists.txt                  # Build configuration
```

## Core Components Technical Design

### 1. Main Entry Point (`main.cpp`)

**Responsibilities**:
- Minimal initialization sequence
- Command-line argument parsing (simple, no library)
- Signal handler setup (SIGTERM, SIGINT)
- Single event loop execution

**Technical Decisions**:
- Direct system calls for critical setup (avoid libc overhead)
- Stack-allocated configuration structure
- Immediate privilege dropping after socket bind
- No dynamic memory allocation in main

**Optimization Rationale**:
- Reduce startup time by avoiding unnecessary initialization
- Static initialization order carefully controlled
- No exception handling (terminate on critical errors)

### 2. VSocket Server Module

#### `vsocket/server.hpp/cpp`

**Functionality**:
- Create AF_VSOCK socket with non-blocking mode
- Bind to specified CID and port
- Listen with minimal backlog (1-2 connections)
- Integrate with epoll for event notification

**Technical Implementation**:
```cpp
class VSocketServer {
private:
    int socket_fd_;
    int epoll_fd_;
    static constexpr int MAX_EVENTS = 10;
    epoll_event events_[MAX_EVENTS];  // Stack allocated
    
    // Pre-allocated accept buffer
    sockaddr_vm client_addr_;
    socklen_t client_addr_len_;
};
```

**Optimizations**:
- `SOCK_NONBLOCK | SOCK_CLOEXEC` flags on socket creation
- Edge-triggered epoll (EPOLLET) for efficiency
- Pre-allocated event array (no heap allocation)
- Inline accept path for hot code

#### `vsocket/connection.hpp/cpp`

**Functionality**:
- Handle single active connection
- Message framing with length prefix
- Zero-copy receive into pre-allocated buffers
- Vectored I/O for multi-part sends

**Technical Implementation**:
```cpp
class Connection {
private:
    int fd_;
    
    // Pre-allocated buffers
    static constexpr size_t RECV_BUFFER_SIZE = 65536;
    static constexpr size_t SEND_BUFFER_SIZE = 131072;
    
    alignas(64) uint8_t recv_buffer_[RECV_BUFFER_SIZE];
    alignas(64) uint8_t send_buffer_[SEND_BUFFER_SIZE];
    
    // Ring buffer indices for zero-copy
    size_t recv_head_ = 0;
    size_t recv_tail_ = 0;
    
    // Message framing
    enum class ReadState {
        READING_LENGTH,
        READING_BODY
    };
};
```

**Optimizations**:
- Cache-aligned buffers for better performance
- Ring buffer to avoid memmove operations
- State machine for efficient parsing
- Scatter-gather I/O with `readv`/`writev`

### 3. Protocol Module

#### `protocol/message.hpp`

**Design Decisions**:
- POD structures for zero-cost abstraction
- Fixed-size strings where possible
- Enum classes for type safety
- No virtual functions (avoid vtable)

```cpp
struct ExecutionRequest {
    char id[37];  // UUID + null terminator
    Language language;
    uint32_t timeout_ms;
    uint32_t memory_limit_bytes;
    // Code and stdin stored as string_views into parse buffer
};
```

#### `protocol/parser.hpp/cpp`

**simdjson Integration**:
- On-demand parsing (no DOM building)
- Parse directly from connection buffer
- Validate while parsing (single pass)
- Pre-compiled JSON paths

**Optimization Techniques**:
- Template-based parsing for compile-time optimization
- Likely/unlikely branch hints
- Minimal string copying (use string_view)
- Error codes instead of exceptions

#### `protocol/serializer.hpp/cpp`

**Fast JSON Building**:
- Pre-allocated format strings
- Fixed-point number formatting
- SIMD-accelerated base64 encoding
- Single-pass buffer writing

```cpp
class Serializer {
private:
    // Pre-computed JSON templates
    static constexpr const char* ACK_TEMPLATE = 
        R"({"id":"%s","type":"ack","status":"received"})";
    
    // Stack buffer for number formatting
    char number_buffer_[32];
    
    // Optimized integer to string conversion
    char* format_uint64(uint64_t value, char* buffer);
};
```

### 4. Executor Module

#### `executor/executor.hpp/cpp`

**Core Execution Logic**:
- Language-agnostic interface
- Resource limit setup
- Timeout management
- Output collection

**Technical Architecture**:
```cpp
class Executor {
protected:
    // Pre-created pipes (avoid repeated pipe() calls)
    int stdout_pipe_[2];
    int stderr_pipe_[2];
    
    // Pre-allocated output buffers
    static constexpr size_t OUTPUT_BUFFER_SIZE = 1048576;  // 1MB
    uint8_t stdout_buffer_[OUTPUT_BUFFER_SIZE];
    uint8_t stderr_buffer_[OUTPUT_BUFFER_SIZE];
    
    // Efficient child process management
    pid_t child_pid_ = -1;
    
    // Resource tracking
    struct rusage child_rusage_;
};
```

**Optimization Strategies**:
- Pre-fork process pool for compiled languages
- Splice system call for zero-copy output capture
- Non-blocking pipe reads
- Batch system calls where possible

#### `executor/process.hpp/cpp`

**Process Management**:
- Efficient fork/exec patterns
- Signal-safe child handling
- Resource limit enforcement
- Zombie process prevention

**Technical Optimizations**:
- `vfork()` instead of `fork()` where safe
- Pre-built argv arrays
- Close-on-exec for all file descriptors
- Minimal work between fork and exec

#### Language-Specific Implementations

**Common Patterns**:
- Static compiler flags arrays
- Pre-allocated temporary file paths
- Cached compiler locations
- Language-specific optimizations

**Python** (`languages/python.cpp`):
- Direct execution without intermediate files
- `-u` flag for unbuffered output
- Import restriction via `-s` flag

**TypeScript** (`languages/typescript.cpp`):
- esbuild for fast transpilation
- Bundle mode to avoid module issues
- Cached node_modules (read-only bind mount)

**C++** (`languages/cpp.cpp`):
- Aggressive optimization flags (-O2)
- Static linking to avoid runtime dependencies
- Precompiled headers for common includes

**C#** (`languages/csharp.cpp`):
- AOT compilation where available
- Minimal runtime configuration
- Single-file publishing

**Rust** (`languages/rust.cpp`):
- Release mode with optimizations
- Minimal standard library
- Cached cargo registry (read-only)

### 5. Monitor Module

#### `monitor/resource_monitor.hpp/cpp`

**Efficient Resource Tracking**:
- Timer-based sampling (not continuous)
- Direct /proc parsing (no libraries)
- Minimal string parsing
- Statistical sampling for accuracy

```cpp
class ResourceMonitor {
private:
    // Pre-allocated parse buffers
    char stat_buffer_[512];
    char status_buffer_[4096];
    
    // Efficient /proc/[pid]/stat parsing
    struct ProcStat {
        uint64_t utime;
        uint64_t stime;
        uint64_t vsize;
        long rss;
    };
    
    // Parse without string splitting
    bool parse_stat_line(const char* line, ProcStat& stat);
};
```

**Optimization Techniques**:
- Single read() per sample
- Manual parsing (no sscanf)
- Branch-free parsing where possible
- Cached file descriptors

### 6. Utilities Module

#### `utils/buffer_pool.hpp/cpp`

**Memory Management**:
- Pre-allocated buffer pools
- Power-of-2 sizing for fast indexing
- Lock-free allocation (single-threaded)
- Automatic reset between requests

#### `utils/base64.hpp/cpp`

**SIMD Optimization**:
- AVX2 implementation for x86-64
- Fallback scalar implementation
- Lookup table optimization
- In-place encoding/decoding

#### `utils/logger.hpp`

**Compile-Time Logging**:
```cpp
#ifdef DEBUG_BUILD
#define LOG_DEBUG(fmt, ...) \
    logger::log(LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) ((void)0)
#endif
```

## Memory Layout and Optimization

### Stack Usage Strategy
- Large buffers on heap (pre-allocated)
- Small buffers on stack (< 4KB)
- Careful stack depth management
- Guard pages for overflow detection

### Memory Alignment
- Cache line alignment (64 bytes) for hot data
- Page alignment for large buffers
- Packed structures where appropriate
- Padding to avoid false sharing

### Zero-Copy Techniques
1. **Input Path**: Parse JSON directly from socket buffer
2. **Code Writing**: Write from socket buffer to file
3. **Output Collection**: Splice from pipe to memory
4. **Response Path**: Build JSON in send buffer

## Concurrency Model

### Single-Threaded Design
**Rationale**:
- One execution per VM (no need for parallelism)
- Simpler state management
- No synchronization overhead
- Predictable performance

### Event-Driven Architecture
```
┌─────────────┐
│ Event Loop  │
└──────┬──────┘
       │
┌──────▼──────┐     ┌────────────┐     ┌──────────┐
│   epoll     │────►│  Socket    │────►│ Process  │
│   wait      │     │  Events    │     │ Events   │
└──────┬──────┘     └────────────┘     └──────────┘
       │
┌──────▼──────┐     ┌────────────┐     ┌──────────┐
│   Timer     │────►│  Timeout   │────►│ Resource │
│   Events    │     │  Handler   │     │ Sample   │
└─────────────┘     └────────────┘     └──────────┘
```

## Build Configuration

### Compiler Flags
```cmake
# Release build flags
set(CMAKE_CXX_FLAGS_RELEASE 
    "-O3 -march=x86-64-v2 -mtune=generic"
    "-flto -fno-exceptions -fno-rtti"
    "-ffunction-sections -fdata-sections"
    "-fvisibility=hidden -fvisibility-inlines-hidden"
    "-fno-semantic-interposition"
    "-fmerge-all-constants")

# Security flags
set(SECURITY_FLAGS
    "-D_FORTIFY_SOURCE=2"
    "-fstack-protector-strong"
    "-fPIE -Wl,-pie"
    "-Wl,-z,relro -Wl,-z,now"
    "-Wl,--strip-all")
```

### Static Linking Strategy
- Static libstdc++ and libgcc
- Dynamic libc (for nsswitch compatibility)
- Static simdjson
- Minimal binary size (target < 500KB)

## Error Handling Philosophy

### Fail-Fast Approach
- Critical errors → immediate termination
- User errors → error response
- System errors → logged and handled
- No exception usage (performance)

### Error Propagation
```cpp
enum class ErrorCode : uint8_t {
    SUCCESS = 0,
    INVALID_JSON,
    UNSUPPORTED_LANGUAGE,
    FORK_FAILED,
    TIMEOUT,
    MEMORY_EXCEEDED,
    // ... etc
};

// Result type for error handling
template<typename T>
struct Result {
    T value;
    ErrorCode error;
    
    bool ok() const { return error == ErrorCode::SUCCESS; }
};
```

## Performance Targets and Measurement

### Microbenchmarks
1. **JSON Parsing**: < 10μs for typical request
2. **Process Spawn**: < 5ms overhead
3. **Output Collection**: > 1GB/s throughput
4. **Base64 Encoding**: > 2GB/s with SIMD

### System Benchmarks
1. **Cold Start**: < 50ms to ready state
2. **Request Latency**: < 2ms processing overhead
3. **Memory Usage**: < 10MB RSS baseline
4. **CPU Usage**: < 1% when idle

### Profiling Integration
- Conditional compilation for profiling
- Built-in timing points
- Memory usage tracking
- CPU cycle counting (RDTSC)

## Security Hardening

### Compile-Time Security
- Stack canaries enabled
- Control flow integrity (CFI)
- Integer overflow checking
- Format string checking

### Runtime Security
- ASLR enabled
- W^X enforcement
- Syscall filtering (seccomp-bpf)
- Capability dropping

### Input Validation
- Size limits enforced first
- Type checking second
- Content validation last
- Fail closed on errors

## Testing Infrastructure

### Unit Test Strategy
- Header-only test utilities
- Minimal test framework
- Mock system calls
- Deterministic timing

### Fuzz Testing
- AFL++ integration
- Protocol fuzzing
- Language-specific fuzzing
- Resource exhaustion testing

### Performance Testing
- Automated benchmarks
- Regression detection
- Profile-guided optimization
- Memory leak detection

## Deployment Considerations

### Binary Distribution
- Single static binary
- Strip symbols for size
- Compress with UPX (optional)
- Checksum verification

### Runtime Requirements
- Linux kernel 4.14+ (AF_VSOCK support)
- No external dependencies
- Minimal filesystem access
- No network requirements

### Configuration
- Command-line only (no config files)
- Environment variables for debugging
- Compile-time feature flags
- Runtime capability detection