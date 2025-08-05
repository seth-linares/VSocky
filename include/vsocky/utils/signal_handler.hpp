#pragma once

#include <atomic>
#include <csignal>
#include <print>

// =============================================================================
// UNDERSTANDING std::atomic AND MEMORY ORDERING
// =============================================================================
// std::atomic provides thread-safe operations on primitive types without locks.
// 
// WHY WE NEED IT:
// In multi-threaded programs (or with signal handlers), multiple threads might
// try to read/write the same variable simultaneously. Without atomics:
//   Thread 1: shutdown = true;   // Writes first byte
//   Thread 2: if (shutdown) ...  // Reads partially written value!
//   Thread 1: (continues writing remaining bytes)
//
// std::atomic guarantees that operations complete atomically (all-or-nothing).
//
// MEMORY ORDERING:
// Modern CPUs reorder instructions for performance. Memory ordering controls
// how operations are visible across threads:
//
// - memory_order_relaxed: No ordering guarantees (rarely used)
// - memory_order_acquire: All reads/writes after this see updated values
// - memory_order_release: All reads/writes before this are visible to acquire
// - memory_order_seq_cst: Strictest ordering (default, but slower)
//
// We use acquire/release because:
// 1. Writer (signal handler): shutdown.store(true, release) 
//    → Ensures all previous operations complete before setting flag
// 2. Reader (main thread): shutdown.load(acquire)
//    → Ensures we see the updated value and any operations before it
// =============================================================================

namespace vsocky {

// Handles SIGTERM and SIGINT for graceful shutdown
class signal_handler {
public:
    // Set up signal handlers - call once at program start
    static void setup();
    
    // Check if shutdown has been requested
    // - noexcept: Promises this function won't throw exceptions
    // - load(): Atomically reads the value
    // - memory_order_acquire: Ensures we see updates from other threads
    static bool should_shutdown() noexcept {
        return shutdown_requested_.load(std::memory_order_acquire);
    }
    
    // Reset shutdown flag (mainly for testing)
    // - store(): Atomically writes the value
    // - memory_order_release: Ensures our write is visible to other threads
    static void reset() noexcept {
        shutdown_requested_.store(false, std::memory_order_release);
    }
    
private:
    // Signal handler function
    // Called by the OS when our process receives a signal
    // CRITICAL: Can only use async-signal-safe functions inside!
    static void handle_signal(int signal);
    
    // Atomic flag for shutdown request
    // - static: Shared across all instances (though we never instantiate this class)
    // - atomic<bool>: Thread-safe boolean that can be safely accessed from signal handlers
    // - Must be defined in the .cpp file (declaration here, definition there)
    static std::atomic<bool> shutdown_requested_;
};

} // namespace vsocky