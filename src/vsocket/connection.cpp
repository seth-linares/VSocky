#include "vsocky/vsocket/connection.hpp"

#include <unistd.h>      // close(), read(), write()
#include <fcntl.h>       // fcntl() for non-blocking mode
#include <sys/socket.h>  // socket operations
#include <linux/vm_sockets.h> // VSock structures
#include <cerrno>        // errno for error checking
#include <utility>       // std::exchange for move semantics

// =============================================================================
// UNDERSTANDING THE LINUX VSOCK API
// =============================================================================
// VSock (Virtual Socket) is a Linux socket family for host-guest communication
// in virtualized environments. Key concepts:
//
// - CID (Context ID): Like an IP address, identifies a VM
//   - VMADDR_CID_ANY (4294967295): Accept from any CID
//   - VMADDR_CID_HOST (2): The host machine
//   - VMADDR_CID_LOCAL (1): Loopback
//   - Guest CIDs: Usually start at 3
//
// - Port: Just like TCP ports, identifies a service (we use 52000)
//
// - Socket operations are identical to TCP once created, just different
//   address family (AF_VSOCK instead of AF_INET)
// =============================================================================

namespace vsocky {

    // =============================================================================
    // CONSTRUCTOR - Taking Ownership
    // =============================================================================
    Connection::Connection(int fd) noexcept : fd_(fd) {
        // Design decision: We accept the fd as-is and only set non-blocking mode
        // We don't validate if it's actually a socket - that's the caller's job
        // This makes the class more flexible (could work with pipes, etc.)
        
        if (fd_ >= 0) {
            // Set non-blocking mode immediately
            // We ignore the error here - the socket might already be non-blocking
            // If it fails, subsequent operations will fail and return error codes
            set_non_blocking();
        }
        
        // Note: We don't check if fd is valid beyond >= 0
        // Invalid fds will cause operations to fail with appropriate error codes
        // This follows the "fail fast" principle - errors surface at point of use
    }

    // =============================================================================
    // DESTRUCTOR - RAII Cleanup
    // =============================================================================
    Connection::~Connection() noexcept {
        // CRITICAL: This destructor is what makes RAII work!
        // Even if an exception is thrown elsewhere, this WILL run
        // This guarantees we never leak file descriptors
        
        close();  // close() checks if fd_ != -1, so always safe to call
        
        // Why we don't check the return value of close():
        // 1. We're in a destructor - we can't throw
        // 2. Even if close() fails, the fd is deallocated by the kernel
        // 3. Common failure (EINTR) isn't relevant during destruction
        // 4. Logging here could cause issues during program termination
    }

    // =============================================================================
    // MOVE CONSTRUCTOR - Transfer Ownership
    // =============================================================================
    Connection::Connection(Connection&& other) noexcept 
        : fd_(std::exchange(other.fd_, -1)) {
        // std::exchange is perfect for move constructors:
        // 1. Returns the old value of other.fd_
        // 2. Sets other.fd_ to -1 (empty state)
        // 3. All in one atomic operation
        //
        // After this:
        // - We own the file descriptor
        // - other is in valid but empty state (fd_ == -1)
        // - other's destructor will see -1 and do nothing
    }

    // =============================================================================
    // MOVE ASSIGNMENT - Transfer with Cleanup
    // =============================================================================
    Connection& Connection::operator=(Connection&& other) noexcept {
        // Guard against self-assignment: conn = std::move(conn);
        // This would close our fd and then try to use it!
        if (this != &other) {
            // First: Clean up our current resource (if any)
            close();
            
            // Then: Take ownership of other's resource
            fd_ = std::exchange(other.fd_, -1);
        }
        
        return *this;  // Enable chaining: a = b = c;
    }

    // =============================================================================
    // READ OPERATION - Receiving Data
    // =============================================================================
    std::error_code Connection::read(std::span<uint8_t> buffer, size_t& bytes_read) noexcept {
        bytes_read = 0;  // Always initialize output parameters
        
        if (fd_ == -1) {
            return error_code::connection_closed;
        }
        
        if (buffer.empty()) {
            return error_code::success;  // Nothing to read into
        }
        
        // ==========================================================================
        // THE POSIX read() SYSTEM CALL
        // ==========================================================================
        // ssize_t read(int fd, void *buf, size_t count);
        //
        // Returns:
        //  > 0: Number of bytes read
        //  = 0: End of file (peer closed connection)
        //  < 0: Error occurred, check errno
        //
        // In non-blocking mode, additional behavior:
        //  EAGAIN/EWOULDBLOCK: No data available right now
        //  EINTR: Interrupted by signal (even with SA_RESTART)
        // ==========================================================================
        
        ssize_t result = ::read(fd_, buffer.data(), buffer.size());
        
        if (result > 0) {
            // Success: Got some data
            bytes_read = static_cast<size_t>(result);
            return error_code::success;
        } else if (result == 0) {
            // EOF: Peer closed the connection gracefully
            // This is a normal way for connections to end
            return error_code::connection_closed;
        } else {
            // Error: Check errno for details
            // Common errors in non-blocking mode:
            switch (errno) {
                case EAGAIN:      // No data available (non-blocking mode)
                    // This isn't really an error - just no data right now
                    // Caller should try again later
                    return error_code::success;
                    
                case EINTR:
                    // Interrupted by signal - caller should retry
                    // Even with SA_RESTART, some conditions cause EINTR
                    return error_code::interrupted;
                    
                case ECONNRESET:
                    // Connection reset by peer (ungraceful close)
                    // Different from EOF - this is an error condition
                    return error_code::connection_closed;
                    
                case EBADF:
                case ENOTCONN:
                case ENOTSOCK:
                    // Programming errors - fd isn't valid/connected
                    return error_code::read_failed;
                    
                default:
                    // Other errors (EFAULT, EIO, etc.)
                    return error_code::read_failed;
            }
        }
    }

    // =============================================================================
    // WRITE OPERATION - Sending Data
    // =============================================================================
    std::error_code Connection::write(std::span<const uint8_t> data, size_t& bytes_written) noexcept {
        bytes_written = 0;  // Always initialize output parameters
        
        if (fd_ == -1) {
            return error_code::connection_closed;
        }
        
        if (data.empty()) {
            return error_code::success;  // Nothing to write
        }
        
        // ==========================================================================
        // THE POSIX write() SYSTEM CALL
        // ==========================================================================
        // ssize_t write(int fd, const void *buf, size_t count);
        //
        // Returns:
        //  > 0: Number of bytes written (might be less than requested!)
        //  = 0: Nothing written (can happen with non-blocking)
        // < 0: Error occurred
        //
        // CRITICAL: Partial writes are normal!
        // If you request to write 1000 bytes, write() might only accept 500.
        // The caller must check bytes_written and retry with remaining data.
        // ==========================================================================
        
        ssize_t result = ::write(fd_, data.data(), data.size());
        
        if (result >= 0) {
            // Success: Wrote some data (possibly 0 bytes in non-blocking mode)
            bytes_written = static_cast<size_t>(result);
            return error_code::success;
        } else {
            // Error: Check errno for details
            switch (errno) {
                case EAGAIN:
                    // Output buffer is full - caller should try again later
                    // This is normal in non-blocking mode
                    return error_code::success;
                    
                case EINTR:
                    // Interrupted by signal
                    return error_code::interrupted;
                    
                case EPIPE:
                case ECONNRESET:
                    // Peer closed the connection
                    // EPIPE: Writing to a closed pipe/socket
                    // ECONNRESET: Connection reset by peer
                    return error_code::connection_closed;
                    
                case EBADF:
                case ENOTCONN:
                case ENOTSOCK:
                    // Programming errors
                    return error_code::write_failed;
                    
                default:
                    // Other errors (EFAULT, EIO, ENOSPC, etc.)
                    return error_code::write_failed;
            }
        }
    }

    // =============================================================================
    // SET NON-BLOCKING MODE
    // =============================================================================
    std::error_code Connection::set_non_blocking() noexcept {
        if (fd_ == -1) {
            return error_code::connection_closed;
        }
        
        // ==========================================================================
        // UNDERSTANDING fcntl() AND FILE DESCRIPTOR FLAGS
        // ==========================================================================
        // fcntl (file control) is a Swiss Army knife for file descriptors.
        // 
        // F_GETFL: Get current flags
        // F_SETFL: Set flags (only certain flags can be changed)
        // O_NONBLOCK: Non-blocking I/O flag
        //
        // Process:
        // 1. Get current flags
        // 2. OR in O_NONBLOCK to add it
        // 3. Set the modified flags
        // ==========================================================================
        
        // Get current flags
        int flags = ::fcntl(fd_, F_GETFL, 0);
        if (flags == -1) {
            return error_code::internal_error;
        }
        
        // Add non-blocking flag if not already set
        if (!(flags & O_NONBLOCK)) {
            if (::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
                return error_code::internal_error;
            }
        }
        
        return error_code::success;
    }

    // =============================================================================
    // GET PEER CID (Context ID)
    // =============================================================================
    std::optional<uint32_t> Connection::peer_cid() const noexcept {
        if (fd_ == -1) {
            return std::nullopt;
        }
        
        // ==========================================================================
        // GETPEERNAME FOR VSOCK
        // ==========================================================================
        // getpeername() retrieves the address of the peer (remote end).
        // For VSock, this gives us the CID and port of the connected VM.
        //
        // struct sockaddr_vm {
        //     sa_family_t svm_family;  // AF_VSOCK
        //     uint16_t svm_reserved1;
        //     uint32_t svm_port;
        //     uint32_t svm_cid;
        //     uint8_t svm_zero[4];
        // };
        // ==========================================================================
        
        struct sockaddr_vm peer_addr{};
        socklen_t addr_len = sizeof(peer_addr);
        
        if (::getpeername(fd_, reinterpret_cast<struct sockaddr*>(&peer_addr), &addr_len) == 0) {
            return peer_addr.svm_cid;
        }
        
        return std::nullopt;
    }

    // =============================================================================
    // GET PEER PORT
    // =============================================================================
    std::optional<uint32_t> Connection::peer_port() const noexcept {
        if (fd_ == -1) {
            return std::nullopt;
        }
        
        struct sockaddr_vm peer_addr{};
        socklen_t addr_len = sizeof(peer_addr);
        
        if (::getpeername(fd_, reinterpret_cast<struct sockaddr*>(&peer_addr), &addr_len) == 0) {
            return peer_addr.svm_port;
        }
        
        return std::nullopt;
    }

    // =============================================================================
    // CLOSE CONNECTION
    // =============================================================================
    void Connection::close() noexcept {
        if (fd_ != -1) {
            // =======================================================================
            // THE close() SYSTEM CALL
            // =======================================================================
            // close() decrements the file descriptor's reference count.
            // When it reaches 0, the kernel releases the resource.
            //
            // For sockets, this initiates graceful shutdown (TCP FIN, etc.)
            //
            // Common errors:
            // - EBADF: Invalid fd (we prevent this with our check)
            // - EINTR: Interrupted (we ignore - fd is still closed)
            // - EIO: I/O error (we ignore - nothing we can do)
            // =======================================================================
            
            ::close(fd_);  // Ignore return value - see destructor comments
            fd_ = -1;      // Mark as closed to prevent double-close
        }
    }

} // namespace vsocky