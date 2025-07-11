# VSocket Implementation Plan for Firecracker Code Execution

## Executive Summary

This plan outlines the development of `vsocky`, a lightweight VSocket server that runs inside Firecracker VMs to securely execute user-submitted code. The system will handle communication between a Go microservice (host) and Alpine Linux VMs (guests), supporting Python, TypeScript, C#, C++, and Rust execution with minimal overhead.

## Architecture Overview

### System Components

```
┌─────────────────────────────────────────────────────────────────┐
│                     Go Microservice (Host)                       │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────────────┐ │
│  │ Firecracker │  │   VSocket    │  │   Execution Manager   │ │
│  │     SDK     │  │   Client     │  │  (Language Runtimes)  │ │
│  └─────────────┘  └──────────────┘  └───────────────────────┘ │
└────────────────────────────┬────────────────────────────────────┘
                             │ VSocket Connection
                             │ (CID:Port)
┌────────────────────────────┴────────────────────────────────────┐
│                    Firecracker VM (Guest)                        │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │                    vsocky (C++)                          │  │
│  │  ┌──────────┐  ┌──────────┐  ┌───────────────────────┐ │  │
│  │  │ VSocket  │  │  JSON    │  │   Process Executor    │ │  │
│  │  │  Server  │  │ Protocol │  │  & Resource Monitor   │ │  │
│  │  └──────────┘  └──────────┘  └───────────────────────┘ │  │
│  └─────────────────────────────────────────────────────────┘  │
│                         Alpine Linux                            │
└─────────────────────────────────────────────────────────────────┘
```

### Communication Flow

1. **Connection Establishment**: VM boots → vsocky starts → listens on predefined VSocket port
2. **Code Submission**: Go service connects → sends execution request → vsocky acknowledges
3. **Execution**: vsocky creates sandbox → executes code → monitors resources
4. **Result Return**: vsocky collects output → formats response → sends via VSocket
5. **Cleanup**: Connection closes → VM can be destroyed or reused

## Core Components to Build

### 1. VSocket Server Foundation

**Purpose**: Establish reliable communication channel between host and VM

**Functionality**:
- Non-blocking VSocket server listening on configurable port (default: 52000)
- Single connection handling (one execution per VM)
- Event-driven architecture using epoll for efficiency
- Connection state management (waiting, connected, executing, closing)
- Graceful shutdown handling

**Key Design Decisions**:
- Single-threaded event loop (simpler, sufficient for single execution)
- Pre-allocated buffers to minimize allocations
- Zero-copy message handling where possible

### 2. JSON Message Protocol

**Purpose**: Structured, extensible communication format

**Message Types**:

```json
// Execution Request (Host → VM)
{
    "id": "unique-request-id",
    "type": "execute",
    "language": "python|typescript|csharp|cpp|rust",
    "code": "base64-encoded-source-code",
    "timeout": 30000,  // milliseconds
    "memory_limit": 134217728,  // bytes (128MB)
    "stdin": "optional-base64-encoded-input"
}

// Acknowledgment (VM → Host)
{
    "id": "unique-request-id",
    "type": "ack",
    "status": "received"
}

// Execution Result (VM → Host)
{
    "id": "unique-request-id",
    "type": "result",
    "status": "success|timeout|memory_exceeded|runtime_error|compilation_error",
    "stdout": "base64-encoded-output",
    "stderr": "base64-encoded-error",
    "exit_code": 0,
    "execution_time": 1234,  // milliseconds
    "memory_used": 12345678,  // bytes
    "compilation_time": 567  // milliseconds (for compiled languages)
}

// Error Response (VM → Host)
{
    "id": "unique-request-id",
    "type": "error",
    "error": "invalid_request|internal_error|unsupported_language",
    "message": "Human-readable error description"
}
```

**Implementation Details**:
- Use simdjson for parsing (performance critical)
- Validate all fields strictly
- Base64 encoding for code/output to handle special characters
- Length-prefixed messages for reliable framing

### 3. Language Execution Engine

**Purpose**: Safely execute code in multiple languages with consistent behavior

**Language-Specific Handlers**:

1. **Python**
   - Direct execution via `python3`
   - No compilation step
   - Capture imports for security analysis

2. **TypeScript**
   - Transpile with pre-installed `tsc` or `esbuild`
   - Execute with `node`
   - Handle module resolution

3. **C#**
   - Compile with `dotnet` or `mcs`
   - Execute resulting binary
   - Handle .NET runtime setup

4. **C++**
   - Compile with `g++` (C++20 standard)
   - Link statically for portability
   - Execute resulting binary

5. **Rust**
   - Compile with `rustc`
   - Handle cargo dependencies (restricted)
   - Execute resulting binary

**Common Execution Features**:
- Sandbox directory per execution
- Input/output redirection
- Process isolation (separate PID namespace if possible)
- Signal handling for timeouts
- Resource limit enforcement

### 4. Resource Management & Monitoring

**Purpose**: Enforce limits and collect metrics without impacting performance

**Resource Limits**:
- CPU time limit (via setrlimit)
- Wall clock timeout (via timer)
- Memory limit (via cgroups or setrlimit)
- Process count limit
- File descriptor limit
- Disk usage limit (sandbox size)

**Monitoring Strategy**:
- Periodic sampling (100ms intervals)
- /proc/[pid]/stat for CPU usage
- /proc/[pid]/status for memory usage
- Minimal overhead approach
- Stop monitoring on process exit

**Metrics Collection**:
- Peak memory usage
- Total CPU time
- Wall clock time
- System vs user time split

### 5. Security Sandboxing

**Purpose**: Prevent malicious code from escaping or damaging the VM

**Sandbox Layers**:
1. **Filesystem Isolation**
   - Chroot to temporary directory
   - Read-only bind mounts for interpreters/compilers
   - No network filesystem access

2. **Process Restrictions**
   - Drop all capabilities after startup
   - setuid to unprivileged user
   - Restrict system calls (if seccomp available)

3. **Network Isolation**
   - No network access for executed code
   - Only vsocky has VSocket access

4. **Resource Isolation**
   - Separate PID namespace (if available)
   - CPU and memory limits
   - No access to host devices

### 6. Error Handling & Resilience

**Purpose**: Graceful handling of all failure modes

**Error Categories**:
1. **Protocol Errors**: Invalid JSON, missing fields, unsupported operations
2. **Execution Errors**: Compilation failures, runtime crashes, resource exhaustion
3. **System Errors**: Fork failures, I/O errors, permission issues
4. **Timeout Errors**: Compilation timeout, execution timeout, total timeout

**Recovery Strategies**:
- Always send response (even on critical errors)
- Clean up resources on any exit path
- Log errors to stderr for debugging
- Never leave zombie processes

### 7. Performance Optimizations

**Purpose**: Minimize latency and resource usage

**Optimization Areas**:
1. **Startup Time**
   - Static binary with minimal dependencies
   - Pre-initialized data structures
   - Immediate VSocket listening

2. **Message Processing**
   - Zero-copy JSON parsing where possible
   - Pre-allocated response buffers
   - Vectored I/O for large messages

3. **Execution Efficiency**
   - Process pool for compiled languages (pre-fork)
   - Compiler output caching (optional)
   - Minimal syscall overhead

4. **Memory Usage**
   - Fixed-size buffers
   - Stack allocation preferred
   - Memory pool for dynamic needs

## Testing Strategy

### Unit Testing Components
1. **JSON Protocol**: Message parsing, validation, serialization
2. **Process Execution**: Command building, output capture, timeout handling
3. **Resource Monitoring**: Limit enforcement, metric collection
4. **Error Handling**: Each error path validated

### Integration Testing
1. **Language Support**: Test program for each supported language
2. **Edge Cases**: Empty code, infinite loops, fork bombs, memory bombs
3. **Stress Testing**: Rapid sequential executions, large outputs
4. **Security Testing**: Escape attempts, resource exhaustion

### Performance Testing
1. **Latency Measurement**: Time from request to first byte of response
2. **Throughput Testing**: Executions per second per VM
3. **Resource Usage**: Memory and CPU overhead of vsocky itself
4. **Scalability**: Behavior with varying code sizes and output volumes

## Configuration & Deployment

### Build Configuration
- **Alpine Target**: musl libc, static linking, minimal size
- **Ubuntu Development**: Dynamic linking OK, debug symbols
- **Optimization Level**: -O3 for production, -O0 for development
- **Security Flags**: Stack protection, FORTIFY_SOURCE, PIE

### Runtime Configuration
- **VSocket Port**: Configurable via command line
- **Resource Limits**: Configurable via command line
- **Logging Level**: Compile-time flag
- **Language Support**: Compile-time feature flags

### VM Image Requirements
- **Base**: Alpine Linux (minimal)
- **Compilers/Interpreters**: Pre-installed for each language
- **Libraries**: Minimal standard libraries only
- **vsocky**: Installed at /usr/local/bin/vsocky
- **Init System**: Direct vsocky execution or minimal init

## Operational Considerations

### Monitoring & Observability
- **Logs**: Structured JSON logs to stderr
- **Metrics**: Execution counts, success rates, timing histograms
- **Health**: Simple health check endpoint (optional)

### Failure Modes
1. **VM Boot Failure**: Detected by connection timeout
2. **vsocky Crash**: Detected by connection drop
3. **Resource Exhaustion**: Graceful degradation with error response
4. **Malicious Code**: Contained within VM, logged for analysis

### Performance Targets
- **Startup Time**: < 50ms from process start to ready
- **Message Latency**: < 5ms for request processing
- **Execution Overhead**: < 10ms added to raw execution time
- **Memory Overhead**: < 10MB RSS for vsocky itself

## Future Enhancements (Post-MVP)

1. **Advanced Features**
   - Multi-file support for complex projects
   - Dependency management for certain languages
   - Incremental compilation caching
   - Custom test harness support

2. **Performance Improvements**
   - eBPF-based resource monitoring
   - io_uring for I/O operations
   - SIMD-accelerated JSON parsing
   - Compiler output caching

3. **Security Enhancements**
   - Full seccomp filtering
   - User namespace isolation
   - SELinux/AppArmor integration
   - Crypto-signed code verification

4. **Operational Features**
   - Prometheus metrics export
   - OpenTelemetry tracing
   - Dynamic configuration reload
   - Graceful upgrade support

## Success Criteria

The implementation will be considered successful when:

1. **Functional**: Executes code in all 5 languages reliably
2. **Secure**: Prevents code escape and resource abuse
3. **Fast**: Adds minimal overhead to code execution
4. **Reliable**: Handles all error cases gracefully
5. **Efficient**: Uses minimal memory and CPU resources
6. **Maintainable**: Clean, documented, testable code
7. **Portable**: Runs on Alpine Linux in Firecracker VMs

## Development Priorities

1. **Core VSocket Communication**: Foundation for everything else
2. **JSON Protocol Implementation**: Enable structured communication
3. **Basic Process Execution**: Python first, then other languages
4. **Resource Management**: Critical for production safety
5. **Security Hardening**: Essential before production use
6. **Performance Optimization**: After functionality is complete
7. **Advanced Features**: Based on production feedback