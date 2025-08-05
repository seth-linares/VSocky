#include "vsocky/utils/signal_handler.hpp"

#include <csignal>
#include <cstring>
#include <print>

// =============================================================================
// UNDERSTANDING SIGNAL HANDLING IN UNIX/LINUX
// =============================================================================
// Signals are software interrupts sent to a process. Common signals:
// - SIGTERM: Polite termination request (e.g., from systemd or kill command)
// - SIGINT: Interrupt from keyboard (Ctrl+C)
// - SIGHUP: Terminal disconnected (historical: "hang up")
// - SIGKILL: Force kill (can't be caught or ignored!)
//
// WHY sigaction INSTEAD OF signal()?
// The old signal() function has portability issues:
// 1. Behavior varies between systems (BSD vs System V)
// 2. Signal handler might be reset to default after first signal
// 3. Can't block other signals during handler execution
// 4. Can't get extended information about the signal
//
// sigaction() provides reliable, portable signal handling.
// =============================================================================

namespace vsocky {

// Static member definition
// This allocates the actual storage for our atomic bool.
// The 'false' in braces is uniform initialization - the modern C++ way
// to initialize objects. For atomic<bool>, this sets initial value to false.
std::atomic<bool> signal_handler::shutdown_requested_{false};

void signal_handler::setup() {
    // =======================================================================
    // UNDERSTANDING struct sigaction
    // =======================================================================
    // In C, we must write 'struct sigaction' (not just 'sigaction') because
    // C has separate namespaces for struct tags and regular identifiers.
    // In C++, we can omit 'struct', but it's often kept for C library structs.
    //
    // sigaction is a struct that describes how to handle a signal:
    // - sa_handler: Function to call when signal arrives
    // - sa_mask: Additional signals to block during handler
    // - sa_flags: Modify behavior (SA_RESTART, SA_SIGINFO, etc.)
    // =======================================================================
    struct sigaction sa;
    
    // =======================================================================
    // WHY ZERO OUT THE STRUCTURE?
    // Stack variables contain garbage values. The sigaction struct has many
    // fields we don't use, and they must be initialized to zero/null.
    // memset() is the C way to zero memory quickly.
    //
    // Modern C++ alternative: struct sigaction sa{};  // Value-initialization
    // But memset is more explicit about our intent here.
    // =======================================================================
    std::memset(&sa, 0, sizeof(sa));
    
    // =======================================================================
    // SETTING THE SIGNAL HANDLER FUNCTION
    // sa_handler is a function pointer. The signature is:
    //   void handler(int signal_number);
    //
    // We use & to get the address of our static function, though in C++
    // the & is optional for function names (they decay to pointers).
    //
    // When a signal arrives, the OS will call our function with the signal
    // number as the argument.
    // =======================================================================
    sa.sa_handler = &signal_handler::handle_signal;
    
    // =======================================================================
    // SA_RESTART FLAG
    // Many system calls (read, write, etc.) can be interrupted by signals.
    // Without SA_RESTART, these calls would fail with errno=EINTR.
    // With SA_RESTART, the OS automatically restarts interrupted calls.
    // This makes our code simpler - we don't need to handle EINTR everywhere.
    // =======================================================================
    sa.sa_flags = SA_RESTART;
    
    // =======================================================================
    // SIGNAL MASK - BLOCKING OTHER SIGNALS
    // While our handler runs, we don't want other signals interrupting it.
    // sigfillset(&sa.sa_mask) adds ALL signals to the mask.
    // These signals will be blocked (queued) until our handler returns.
    //
    // sa_mask exists in the struct (even though we zeroed it) because
    // it's a member. sigfillset() fills this existing member with all
    // signal numbers.
    // =======================================================================
    sigfillset(&sa.sa_mask);
    
    // =======================================================================
    // INSTALLING THE SIGNAL HANDLER
    // sigaction(signal_number, new_action, old_action)
    // - signal_number: Which signal to handle (SIGTERM, etc.)
    // - new_action: Pointer to our sigaction struct
    // - old_action: Pointer to store previous handler (we use nullptr)
    //
    // Returns 0 on success, -1 on error.
    // Common errors: Invalid signal number, trying to catch SIGKILL/SIGSTOP
    // =======================================================================
    if (sigaction(SIGTERM, &sa, nullptr) != 0) {
        std::println(stderr, "Warning: Failed to install SIGTERM handler");
    }
    
    // Install handler for SIGINT (Ctrl+C)
    if (sigaction(SIGINT, &sa, nullptr) != 0) {
        std::println(stderr, "Warning: Failed to install SIGINT handler");
    }
    
    // Also handle SIGHUP (terminal hangup)
    if (sigaction(SIGHUP, &sa, nullptr) != 0) {
        std::println(stderr, "Warning: Failed to install SIGHUP handler");
    }
}

void signal_handler::handle_signal(int signal) {
    // =======================================================================
    // ASYNC-SIGNAL-SAFE FUNCTIONS
    // =======================================================================
    // Signal handlers run asynchronously - they can interrupt your program
    // at ANY point, even in the middle of malloc() or printf()!
    //
    // If the interrupted function holds a lock or is modifying global state,
    // calling it again from the handler would deadlock or corrupt memory.
    //
    // Only async-signal-safe functions are allowed. These include:
    // - Simple system calls: write(), close(), _exit()
    // - Atomic operations on lock-free types
    // - A small set of other functions (see man 7 signal-safety)
    //
    // NOT safe: printf, malloc, pthread_mutex_lock, std::cout, etc.
    //
    // std::atomic with lock-free types IS safe because:
    // 1. Operations complete without locks
    // 2. They're designed for concurrent access
    // 3. The standard explicitly allows it
    // =======================================================================
    
    switch (signal) {
        case SIGTERM:
        case SIGINT:
        case SIGHUP:
            // Atomically set the shutdown flag
            // memory_order_release ensures all operations before this are
            // visible to threads that load() with acquire ordering
            shutdown_requested_.store(true, std::memory_order_release);
            break;
        default:
            // Unexpected signal, ignore
            break;
    }
    
    // =======================================================================
    // [[maybe_unused]] ATTRIBUTE
    // This C++17 attribute tells the compiler: "I know this variable might
    // not be used, don't warn me about it." We capture write()'s return
    // value to avoid compiler warnings, but we can't really handle errors
    // in a signal handler, so we mark it [[maybe_unused]].
    // =======================================================================
    if (signal == SIGINT) {
        const char newline = '\n';
        
        // =================================================================
        // write() vs std::println()
        // =================================================================
        // std::println() is NOT async-signal-safe because:
        // 1. It uses buffered I/O (might allocate memory)
        // 2. It might take locks for thread safety
        // 3. It could call malloc() internally
        //
        // write() is a direct system call that:
        // 1. Goes straight to the kernel
        // 2. Doesn't allocate memory
        // 3. Doesn't take locks
        // 4. Is explicitly listed as async-signal-safe
        //
        // We write a newline because when you press Ctrl+C, the terminal
        // shows "^C" but doesn't move to a new line, making the next
        // prompt look messy.
        // =================================================================
        [[maybe_unused]] auto result = write(STDERR_FILENO, &newline, 1);
    }
}

} // namespace vsocky