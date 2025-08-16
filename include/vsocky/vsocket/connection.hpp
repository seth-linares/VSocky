#pragma once

#include "vsocky/utils/error.hpp"

#include <cstdint>
#include <span>
#include <system_error>
#include <optional>

// =============================================================================
// CONNECTION CLASS DESIGN PHILOSOPHY
// =============================================================================
// The Connection class represents a single VSock connection to a client.
// It follows the RAII (Resource Acquisition Is Initialization) pattern:
// (RAII == when resource is aquired it is destroyed on release)
// - Constructor acquires the resource (takes ownership of file descriptor)
// - Destructor releases the resource (closes the file descriptor)
// - Move operations transfer ownership
// - Copy operations are deleted (can't have two owners of same socket)
//
// This design makes it impossible to leak file descriptors 
// (which is bad bc there are a limited # of fds) - even if an
// exception is thrown, the destructor will run and close the socket.
// =============================================================================

namespace vsocky {

class Connection {
public:
    // =========================================================================
    // CONSTRUCTORS AND DESTRUCTOR
    // =========================================================================
    
    // Primary constructor - takes ownership of a file descriptor
    // - explicit: Prevents accidental implicit conversions from int to Connection
    //   Without explicit: Connection conn = 5; // Would compile but be nonsense!
    //   With explicit: Connection conn(5); // Must be intentional
    explicit Connection(int fd) noexcept;
    
    // Destructor - automatically closes the file descriptor
    // - noexcept: Destructors should never throw (C++ will terminate if they do)
    // - Not virtual: This class isn't designed for inheritance
    ~Connection() noexcept;
    
    // =========================================================================
    // MOVE SEMANTICS (Rule of Five - Part 1)
    // =========================================================================
    // Move operations transfer ownership of the file descriptor from one
    // Connection object to another. After moving, the source object is in
    // a valid but empty state (fd_ == -1).
    
    // Move constructor - used for: Connection conn2(std::move(conn1));
    // - noexcept: Promise we won't throw, enables optimizations
    // - After this, other.fd_ will be -1 (empty state)
    Connection(Connection&& other) noexcept;
    
    // Move assignment - used for: conn2 = std::move(conn1);
    // - Must close our current fd (if any) before taking ownership of other's
    // - Returns *this to enable chaining: a = b = c;
    Connection& operator=(Connection&& other) noexcept;
    
    // =========================================================================
    // DELETED OPERATIONS (Rule of Five - Part 2)
    // =========================================================================
    // Copy operations are explicitly deleted because file descriptors can't
    // be shared. Each fd should have exactly one owner.
    
    Connection(const Connection&) = delete;            // No copy constructor
    Connection& operator=(const Connection&) = delete; // No copy assignment
    
    // =========================================================================
    // CORE I/O OPERATIONS
    // =========================================================================
    
    // Read data from the connection into the provided buffer
    // Parameters:
    //   - buffer: Where to store the data (using span for safety)
    //   - bytes_read: Output - how many bytes were actually read
    // Returns:
    //   - success: Data was read successfully
    //   - connection_closed: Peer closed the connection (EOF)
    //   - read_failed: System error (check errno via error_code)
    //   - interrupted: Operation was interrupted (EINTR)
    //
    // ARCHITECTURAL DECISION: Non-blocking I/O
    // We always use non-blocking sockets. If no data is available, read
    // returns immediately with EAGAIN/EWOULDBLOCK rather than blocking.
    // This lets us implement timeouts and check for shutdown signals.
    std::error_code read(std::span<uint8_t> buffer, size_t& bytes_read) noexcept;
    
    // Write data to the connection from the provided buffer
    // Parameters:
    //   - data: The data to send (span provides safety and size)
    //   - bytes_written: Output - how many bytes were actually sent
    // Returns:
    //   - success: Data was sent successfully
    //   - connection_closed: Peer closed the connection (EPIPE)
    //   - write_failed: System error
    //
    // IMPORTANT: Partial writes are possible! If you want to send 1000 bytes,
    // write() might only accept 500. Check bytes_written and loop if needed.
    std::error_code write(std::span<const uint8_t> data, size_t& bytes_written) noexcept;
    
    // =========================================================================
    // UTILITY OPERATIONS
    // =========================================================================
    
    // Check if this Connection object owns a valid file descriptor
    // Returns false after move or if construction failed
    bool is_valid() const noexcept {
        return fd_ != -1;
    }
    
    // Get the underlying file descriptor (for select/poll/epoll if needed)
    // Returns -1 if this is an empty/moved-from Connection
    int fd() const noexcept {
        return fd_;
    }
    
    // Set the socket to non-blocking mode
    // Called automatically by constructor, but exposed for testing
    // Returns error_code if fcntl() fails
    std::error_code set_non_blocking() noexcept;
    
    // =========================================================================
    // CONNECTION INFO
    // =========================================================================
    
    // Get the peer's CID (Context ID - like an IP address for VSock)
    // Returns nullopt if the connection is invalid or getpeername fails
    std::optional<uint32_t> peer_cid() const noexcept;
    
    // Get the peer's port number
    // Returns nullopt if the connection is invalid or getpeername fails
    std::optional<uint32_t> peer_port() const noexcept;
    
    // Close the connection explicitly (also happens in destructor)
    // After calling close(), is_valid() returns false
    // Safe to call multiple times - second call is a no-op
    void close() noexcept;
    
private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================
    
    // The owned file descriptor
    // Invariant: We own this fd and must close it in destructor
    // Special value -1 means "no fd owned" (moved-from or closed state)
    int fd_;
    
    // =========================================================================
    // DESIGN NOTES
    // =========================================================================
    // Why store just an int instead of a fancier type?
    // - File descriptors ARE just ints in POSIX
    // - Keeping it simple makes the class easier to understand
    // - The class interface provides the type safety
    //
    // Why mutable operations are noexcept:
    // - System calls can fail, but we return error codes, not exceptions
    // - This makes our code compatible with environments that disable exceptions
    // - It's also more predictable - you know these operations won't throw
    //
    // Thread safety:
    // - This class is NOT thread-safe
    // - Multiple threads shouldn't call methods on the same Connection
    // - This is by design - thread safety adds overhead and complexity
    // - Higher-level code can add synchronization if needed
};

} // namespace vsocky