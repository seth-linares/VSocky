#include "vsocky/utils/signal_handler.hpp"
#include "vsocky/utils/error.hpp"

#include <thread>
#include <chrono>

// Version info
constexpr const char* VSOCKY_VERSION = "0.1.0";

void print_usage(const char* program_name) {
    std::println("Usage: {} [options]", program_name);
    std::println("Options:");
    std::println("  --version    Show version information");
    std::println("  --help       Show this help message");
    std::println("  --port PORT  VSock port to listen on (default: 52000)");
}

void print_version() {
    std::println("vsocky version {}", VSOCKY_VERSION);
    std::println("Build info:");
#ifdef __VERSION__
    std::println("  Compiler: {}", __VERSION__);
#else
    std::println("  Compiler: Unknown");
#endif
#ifdef HAS_SIMDJSON
    std::println("  JSON parser: simdjson");
#else
    std::println("  JSON parser: none (error!)");
#endif
#ifdef ALPINE_BUILD
    std::println("  Build type: Alpine Linux (musl)");
#else
    std::println("  Build type: Standard Linux (glibc)");
#endif
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    uint16_t port = 52000;
    
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            print_version();
            return 0;
        } else if (arg == "--port" && i + 1 < argc) {
            try {
                port = static_cast<uint16_t>(std::stoi(argv[++i]));
            } catch (...) {
                std::println(stderr, "Error: Invalid port number");
                return 1;
            }
        } else {
            std::println(stderr, "Error: Unknown argument: {}", arg);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Set up signal handling
    vsocky::signal_handler::setup();
    
    std::println("VSocky v{} starting...", VSOCKY_VERSION);
    std::println("Listening on VSock port {}", port);
    
    // TODO: Phase 1 implementation
    // 1. Create VSock server
    // 2. Start main event loop
    // 3. Accept connections
    // 4. Process JSON messages
    // 5. Send responses
    
    std::println("Server implementation coming in Phase 1...");
    
    // Temporary: wait for shutdown signal
    while (!vsocky::signal_handler::should_shutdown()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::println("\nShutting down gracefully...");
    return 0;
}