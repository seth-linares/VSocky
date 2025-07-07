# C++ VSocket Implementation Guide - Efficient Approach

## Core Architecture Principles
- **Single-threaded event loop** (most efficient for single-execution VMs)
- **Stack allocation preferred** (avoid heap fragmentation)
- **Zero-copy operations** where possible
- **Compile-time optimizations** (templates, constexpr)
- **Static linking** for minimal startup time

## Phase 1: Foundation (Startup & Connection)

### 1.1 Minimal Binary Setup
- **Static compilation** with `-static` flag
  - Eliminates dynamic library loading overhead
  - Target binary size: < 500KB stripped
- **Immediate initialization**
  - No configuration file parsing
  - Hard-code constants or use compile-time flags
  - Skip unnecessary system checks
- **Direct system calls** where beneficial
  - Bypass libc wrappers for critical paths
  - Use `syscall()` for vsock operations if needed

### 1.2 VSocket Connection Establishment
- **Pre-allocate all buffers**
  - Single large buffer for all operations (e.g., 64KB)
  - Use segments via pointer arithmetic
  - Avoid repeated allocations
- **Non-blocking I/O setup**
  - Use `epoll` for event notification (most efficient on Linux)
  - Single epoll instance for all file descriptors
  - Edge-triggered mode (EPOLLET) for efficiency
- **Connection acceptance pattern**
  ```cpp
  // Pseudo-code structure
  - socket(AF_VSOCK, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)
  - bind() to VMADDR_CID_ANY and chosen port
  - listen() with minimal backlog (1-2 connections)
  - Add to epoll for incoming connections
  ```

## Phase 2: Protocol Implementation (Efficient Parsing)

### 2.1 JSON Parsing Strategy
- **Single-pass parsing**
  - Use simdjson or RapidJSON for speed
  - Parse directly from receive buffer
  - Avoid intermediate string copies
- **Fixed message schema**
  - Pre-define all possible fields
  - Use static structs for message types
  - Minimize dynamic allocations
- **Memory pooling for strings**
  - Pre-allocate string buffer pool
  - Reset pool between requests
  - Avoid std::string where possible

### 2.2 Message Handling Pipeline
- **Zero-copy message processing**
  - Parse in-place from receive buffer
  - Use string_view for code content
  - Write code directly from buffer to file
- **Immediate acknowledgment**
  - Send ACK before any processing
  - Use pre-formatted response templates
  - Single write() call per response
- **Efficient state machine**
  - Enum-based state tracking
  - Switch statement for state transitions
  - Inline all small functions

## Phase 3: Code Execution (Maximum Efficiency)

### 3.1 Process Management
- **Pre-fork optimization**
  - Fork early, before receiving code
  - Keep child process waiting on pipe
  - Eliminates fork overhead from critical path
- **Direct exec() path**
  - Build argv array on stack
  - Use execve() for precise control
  - No shell involvement (avoid system())
- **File descriptor management**
  - Pre-create pipes for stdout/stderr
  - Use splice() for zero-copy output capture
  - Close unnecessary FDs immediately

### 3.2 Sandbox Setup
- **Minimal filesystem operations**
  - Single directory for all operations
  - Reuse same filenames (no cleanup needed)
  - O_CREAT | O_TRUNC for code files
- **Efficient permissions**
  - Pre-create sandbox directory in rootfs
  - Set permissions once during image build
  - No runtime permission changes

## Phase 4: Resource Monitoring (Low Overhead)

### 4.1 Resource Tracking
- **Sampling-based monitoring**
  - Check resources every 100ms (not continuously)
  - Use timer_create() for periodic checks
  - Read from /proc/[pid]/stat for efficiency
- **Inline limit enforcement**
  - Use setrlimit() in child before exec()
  - RLIMIT_CPU for time limits
  - RLIMIT_AS for memory limits
- **Minimal metrics collection**
  - Track only: CPU time, memory peak, wall time
  - Single getrusage() call after completion
  - Format results during response generation

### 4.2 Timeout Implementation
- **Signalfd-based approach**
  - Add signalfd to epoll loop
  - No signal handlers (avoid interruptions)
  - Clean timeout handling in main loop
- **Efficient child termination**
  - SIGTERM first, SIGKILL after 1 second
  - Reap children immediately (no zombies)
  - Single waitpid() with WNOHANG

## Phase 5: Output Handling (Stream Processing)

### 5.1 Output Capture
- **Ring buffer for output**
  - Fixed-size circular buffer (e.g., 1MB)
  - Overwrite old data if buffer fills
  - Single allocation at startup
- **Splice-based copying** (Linux-specific)
  - splice() from pipe to memory
  - Avoids user-space copying
  - Falls back to read() if splice fails
- **Incremental processing**
  - Process output as it arrives
  - Don't wait for process completion
  - Stream-friendly protocol design

### 5.2 Result Formatting
- **Pre-allocated response buffer**
  - Build JSON response in-place
  - Use fixed-size number formatting
  - Escape strings during copy
- **Single-write response**
  - Build complete response first
  - One write() call to vsocket
  - Avoid partial message sends

## Phase 6: Error Handling (Fail-Fast)

### 6.1 Critical Error Strategy
- **Abort on unrecoverable errors**
  - Out of memory → terminate
  - VSocket errors → terminate
  - Corrupted state → terminate
- **Log and continue for user errors**
  - Invalid code → error response
  - Timeout → killed response
  - Resource limit → limit response

### 6.2 Logging Approach
- **Compile-time log levels**
  - Use macros that compile out
  - No runtime log level checks
  - Structured format for parsing
- **Minimal log output**
  - Errors only in production
  - Debug logs ifdef'd out
  - Write directly to stderr

## Phase 7: Security Hardening (Efficient & Secure)

### 7.1 Input Validation
- **Length checks first**
  - Reject oversized requests immediately
  - Use compile-time constants for limits
  - Single comparison per check
- **Content validation**
  - Whitelist language values (switch statement)
  - Validate timeout ranges (min/max comparison)
  - No regex or complex parsing

### 7.2 Privilege Dropping
- **Immediate after bind()**
  - Drop to nobody:nogroup
  - Close unnecessary capabilities
  - No privilege operations after startup
- **Seccomp filtering** (optional but recommended)
  - Whitelist only required syscalls
  - Apply after initialization
  - Minimal performance impact

## Phase 8: Build & Optimization

### 8.1 Compilation Flags
```bash
CXXFLAGS = -O3 -march=native -flto -fno-exceptions -fno-rtti
CXXFLAGS += -ffunction-sections -fdata-sections
LDFLAGS = -Wl,--gc-sections -static -s
```

### 8.2 Profile-Guided Optimization
- Build with `-fprofile-generate`
- Run typical workloads
- Rebuild with `-fprofile-use`
- 10-20% performance improvement typical

## Performance Targets

### Startup Metrics
- **VM boot to ready**: < 100ms
- **Binary size**: < 500KB stripped
- **Memory usage (idle)**: < 5MB RSS
- **First connection accept**: < 10ms

### Execution Metrics
- **Request to ACK**: < 5ms
- **Code write to disk**: < 10ms
- **Process spawn overhead**: < 20ms
- **Result formatting**: < 5ms

### Resource Limits
- **Max concurrent executions**: 1
- **Max output buffer**: 1MB
- **Max code size**: 64KB
- **Max execution time**: 30s (configurable)

## Implementation Order

1. **Week 1**: Basic vsocket echo server
   - Connection handling
   - Simple message echo
   - Basic error handling

2. **Week 2**: JSON protocol
   - Message parsing
   - Response formatting
   - Protocol state machine

3. **Week 3**: Code execution
   - Process spawning
   - Output capture
   - Timeout handling

4. **Week 4**: Production hardening
   - Resource monitoring
   - Security features
   - Performance optimization

## Testing Strategy

### Unit Testing
- Mock vsocket with Unix sockets
- Test each component in isolation
- Benchmark critical paths

### Integration Testing
- Full VM environment tests
- Stress testing with rapid requests
- Resource exhaustion scenarios

### Performance Testing
- Measure each operation's latency
- Profile with perf tools
- Optimize hotspots

## Code Patterns to Use

### Efficient String Handling
```cpp
// Use string_view for zero-copy parsing
std::string_view extract_code(const char* buffer, size_t len);

// Pre-allocated buffer for formatting
char response_buffer[65536];
int format_response(char* buf, const Result& result);
```

### State Machine Pattern
```cpp
enum class State {
    WAITING_FOR_CONNECTION,
    READING_REQUEST,
    EXECUTING_CODE,
    SENDING_RESPONSE
};

// Tight loop with switch
while (running) {
    switch (state) {
        case State::WAITING_FOR_CONNECTION: ...
    }
}
```

### Resource Guard Pattern
```cpp
// RAII for automatic cleanup
class ProcessGuard {
    pid_t pid;
public:
    ~ProcessGuard() { if (pid > 0) kill(pid, SIGKILL); }
};
```

## Common Pitfalls to Avoid

- Don't use std::regex (too slow)
- Don't use exceptions (overhead)
- Don't use dynamic polymorphism (vtable overhead)
- Don't allocate in hot paths
- Don't use synchronous I/O
- Don't trust user input
- Don't leak file descriptors
- Don't create threads (unnecessary for single execution)

## Debugging Aids

### Conditional Compilation
```cpp
#ifdef DEBUG
#define LOG(fmt, ...) fprintf(stderr, "[%s:%d] " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#else
#define LOG(fmt, ...) ((void)0)
#endif
```

### Performance Markers
```cpp
#ifdef PROFILE
#define TIMED_SECTION(name) ScopedTimer timer(name)
#else
#define TIMED_SECTION(name) ((void)0)
#endif
```