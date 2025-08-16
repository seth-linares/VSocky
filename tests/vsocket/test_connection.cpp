#include "vsocky/vsocket/connection.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include <vector>
#include <thread>
#include <chrono>
#include <csignal>  // For signal()

// =============================================================================
// CONNECTION CLASS UNIT TESTS
// =============================================================================
// These tests verify the Connection class behaves correctly in various
// scenarios. We test RAII behavior, move semantics, I/O operations, and
// error handling.
//
// To run: ./test_connection
// Expected output: All tests should pass with no assertion failures
// =============================================================================

namespace vsocky::test {

// Helper function to create a pair of connected sockets for testing
std::pair<int, int> create_socket_pair() {
    int fds[2];
    // socketpair creates two connected sockets (like a pipe but bidirectional)
    // AF_UNIX because AF_VSOCK doesn't support socketpair
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        throw std::runtime_error("Failed to create socket pair");
    }
    return {fds[0], fds[1]};
}

// =============================================================================
// TEST: RAII and Destructor
// =============================================================================
void test_raii_cleanup() {
    std::cout << "Testing RAII cleanup..." << std::endl;
    
    auto [fd1, fd2] = create_socket_pair();
    std::cout << "  Created socket pair: fd1=" << fd1 << ", fd2=" << fd2 << std::endl;
    
    {
        // Create Connection in a scope
        Connection conn(fd1);
        assert(conn.is_valid());
        assert(conn.fd() == fd1);
        std::cout << "  Connection created with fd=" << fd1 << std::endl;
        
        // Connection goes out of scope here, should close fd1
    }
    std::cout << "  Connection destroyed (out of scope)" << std::endl;
    
    // Try to write to fd2 - should fail since fd1 was closed
    // Note: We need to handle SIGPIPE signal or use MSG_NOSIGNAL
    char buf = 'x';
    ssize_t result = send(fd2, &buf, 1, MSG_NOSIGNAL);
    
    if (result == -1) {
        std::cout << "  Write failed as expected, errno=" << errno 
                  << " (" << strerror(errno) << ")" << std::endl;
        // Could be EPIPE or ECONNRESET depending on timing
        assert(errno == EPIPE || errno == ECONNRESET);
    } else if (result == 1) {
        // Sometimes the write might succeed if kernel buffers the data
        // Try to read response - should fail or return 0 (EOF)
        char read_buf;
        ssize_t read_result = recv(fd2, &read_buf, 1, MSG_DONTWAIT);
        std::cout << "  Write succeeded (buffered), read result=" << read_result << std::endl;
        assert(read_result <= 0);  // Should be 0 (EOF) or -1 (error)
    }
    
    close(fd2);
    std::cout << "✓ RAII cleanup works correctly" << std::endl;
}

// =============================================================================
// TEST: Move Constructor
// =============================================================================
void test_move_constructor() {
    std::cout << "Testing move constructor..." << std::endl;
    
    auto [fd1, fd2] = create_socket_pair();
    
    Connection conn1(fd1);
    assert(conn1.is_valid());
    assert(conn1.fd() == fd1);
    
    // Move construct conn2 from conn1
    Connection conn2(std::move(conn1));
    
    // After move:
    // - conn2 should own the fd
    // - conn1 should be in empty state
    assert(conn2.is_valid());
    assert(conn2.fd() == fd1);
    assert(!conn1.is_valid());
    assert(conn1.fd() == -1);
    
    // conn2 should still be functional
    uint8_t write_buf[] = {'t', 'e', 's', 't'};
    size_t bytes_written = 0;
    auto ec = conn2.write(std::span(write_buf), bytes_written);
    assert(!ec);
    assert(bytes_written == 4);
    
    close(fd2);
    std::cout << "✓ Move constructor transfers ownership correctly" << std::endl;
}

// =============================================================================
// TEST: Move Assignment
// =============================================================================
void test_move_assignment() {
    std::cout << "Testing move assignment..." << std::endl;
    
    auto [fd1, fd2] = create_socket_pair();
    auto [fd3, fd4] = create_socket_pair();
    
    Connection conn1(fd1);
    Connection conn2(fd3);
    
    assert(conn1.fd() == fd1);
    assert(conn2.fd() == fd3);
    std::cout << "  Initial: conn1.fd=" << fd1 << ", conn2.fd=" << fd3 << std::endl;
    
    // Move assign conn1 to conn2
    conn2 = std::move(conn1);
    
    // After move:
    // - conn2 should own fd1 (and have closed fd3)
    // - conn1 should be empty
    assert(conn2.is_valid());
    assert(conn2.fd() == fd1);
    assert(!conn1.is_valid());
    assert(conn1.fd() == -1);
    std::cout << "  After move: conn2.fd=" << conn2.fd() << ", conn1.fd=" << conn1.fd() << std::endl;
    
    // fd3 should have been closed by the assignment
    char buf = 'x';
    ssize_t result = send(fd4, &buf, 1, MSG_NOSIGNAL);
    
    if (result == -1) {
        std::cout << "  Write to fd4 failed as expected, errno=" << errno << std::endl;
        assert(errno == EPIPE || errno == ECONNRESET);
    } else {
        // Write might succeed if buffered
        char read_buf;
        ssize_t read_result = recv(fd4, &read_buf, 1, MSG_DONTWAIT);
        std::cout << "  Write to fd4 succeeded (buffered), read result=" << read_result << std::endl;
        assert(read_result <= 0);  // Should indicate closed connection
    }
    
    close(fd2);
    close(fd4);
    std::cout << "✓ Move assignment closes old fd and transfers ownership" << std::endl;
}

// =============================================================================
// TEST: Read and Write Operations
// =============================================================================
void test_read_write() {
    std::cout << "Testing read/write operations..." << std::endl;
    
    auto [fd1, fd2] = create_socket_pair();
    
    Connection writer(fd1);
    Connection reader(fd2);
    
    // Set both to non-blocking
    assert(!writer.set_non_blocking());
    assert(!reader.set_non_blocking());
    
    // Test 1: Simple write and read
    {
        uint8_t write_data[] = {'H', 'e', 'l', 'l', 'o'};
        size_t bytes_written = 0;
        auto ec = writer.write(std::span(write_data), bytes_written);
        assert(!ec);
        assert(bytes_written == 5);
        
        uint8_t read_buffer[10] = {0};
        size_t bytes_read = 0;
        ec = reader.read(std::span(read_buffer), bytes_read);
        assert(!ec);
        assert(bytes_read == 5);
        assert(memcmp(read_buffer, "Hello", 5) == 0);
    }
    
    // Test 2: Read with no data available (non-blocking behavior)
    {
        uint8_t buffer[10];
        size_t bytes_read = 0;
        auto ec = reader.read(std::span(buffer), bytes_read);
        assert(!ec);  // Should return success with 0 bytes (EAGAIN handled internally)
        assert(bytes_read == 0);
    }
    
    // Test 3: Large write (test partial writes)
    {
        std::vector<uint8_t> large_data(100000, 'X');  // 100KB of 'X'
        size_t total_written = 0;
        
        // Keep writing until all data is sent
        while (total_written < large_data.size()) {
            size_t bytes_written = 0;
            auto remaining = std::span(large_data).subspan(total_written);
            auto ec = writer.write(remaining, bytes_written);
            
            if (ec) {
                assert(false && "Write should not fail");
            }
            
            if (bytes_written == 0) {
                // Buffer full, would need to wait in real scenario
                // For testing, just give the reader a chance to consume
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else {
                total_written += bytes_written;
            }
        }
        
        assert(total_written == large_data.size());
        std::cout << "  - Successfully wrote 100KB (possibly in multiple chunks)" << std::endl;
    }
    
    std::cout << "✓ Read/write operations work correctly" << std::endl;
}

// =============================================================================
// TEST: Connection Closure Detection
// =============================================================================
void test_connection_closure() {
    std::cout << "Testing connection closure detection..." << std::endl;
    
    auto [fd1, fd2] = create_socket_pair();
    
    Connection conn1(fd1);
    Connection conn2(fd2);
    
    // Close conn1
    conn1.close();
    assert(!conn1.is_valid());
    std::cout << "  Closed conn1" << std::endl;
    
    // Try to read from conn2 - should detect EOF immediately
    uint8_t buffer[10];
    size_t bytes_read = 0;
    auto ec = conn2.read(std::span(buffer), bytes_read);
    std::cout << "  Read result: ec=" << ec.message() << ", bytes_read=" << bytes_read << std::endl;
    assert(ec == error_code::connection_closed || bytes_read == 0);
    
    // Try to write from conn2 - should detect closed connection
    // Note: First write might succeed due to buffering
    uint8_t data[] = {'t', 'e', 's', 't'};
    size_t bytes_written = 0;
    ec = conn2.write(std::span(data), bytes_written);
    
    if (!ec && bytes_written > 0) {
        // First write succeeded (buffered), try again to force error
        std::cout << "  First write succeeded (buffered), trying again..." << std::endl;
        ec = conn2.write(std::span(data), bytes_written);
    }
    
    std::cout << "  Write result: ec=" << ec.message() << std::endl;
    assert(ec == error_code::connection_closed || ec == error_code::write_failed);
    
    std::cout << "✓ Connection closure is properly detected" << std::endl;
}

// =============================================================================
// TEST: Error Handling for Invalid FD
// =============================================================================
void test_invalid_fd_handling() {
    std::cout << "Testing invalid fd handling..." << std::endl;
    
    // Test with explicitly invalid fd
    Connection conn(-1);
    assert(!conn.is_valid());
    
    uint8_t buffer[10];
    size_t bytes_read = 0;
    auto ec = conn.read(std::span(buffer), bytes_read);
    assert(ec == error_code::connection_closed);
    
    size_t bytes_written = 0;
    ec = conn.write(std::span(buffer), bytes_written);
    assert(ec == error_code::connection_closed);
    
    // Test with fd that's not a socket (stdin)
    Connection stdin_conn(0);  // fd 0 is stdin
    ec = stdin_conn.set_non_blocking();
    // This might succeed since stdin can be set non-blocking
    // But socket operations would fail
    
    std::cout << "✓ Invalid fd handling works correctly" << std::endl;
}

// =============================================================================
// TEST: Self-Assignment Safety
// =============================================================================
void test_self_assignment() {
    std::cout << "Testing self-assignment safety..." << std::endl;
    
    auto [fd1, fd2] = create_socket_pair();
    
    Connection conn(fd1);
    assert(conn.is_valid());
    
    // Self-assignment should be safe (no-op)
    conn = std::move(conn);
    
    // Connection should still be valid and functional
    assert(conn.is_valid());
    assert(conn.fd() == fd1);
    
    uint8_t data[] = {'o', 'k'};
    size_t bytes_written = 0;
    auto ec = conn.write(std::span(data), bytes_written);
    assert(!ec);
    assert(bytes_written == 2);
    
    close(fd2);
    std::cout << "✓ Self-assignment is handled safely" << std::endl;
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================
void run_all_tests() {
    std::cout << "\n=== Running Connection Class Tests ===" << std::endl;
    
    test_raii_cleanup();
    test_move_constructor();
    test_move_assignment();
    test_read_write();
    test_connection_closure();
    test_invalid_fd_handling();
    test_self_assignment();
    
    std::cout << "\n=== All Tests Passed! ===" << std::endl;
}

} // namespace vsocky::test

int main() {
    // Ignore SIGPIPE globally - we handle EPIPE errors instead
    // This prevents the program from being terminated when writing to closed sockets
    signal(SIGPIPE, SIG_IGN);
    
    try {
        vsocky::test::run_all_tests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}