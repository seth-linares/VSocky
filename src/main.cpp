#include <iostream>
#include <string>
#include <string_view>
#include <cstdlib>
#include <chrono>
#include <print>

#ifdef HAS_SIMDJSON
#include <simdjson.h>
#endif

// Version info
constexpr const char* VSOCKY_VERSION = "0.1.0";

void print_usage(const char* program_name) {
    std::println("Usage: {} [options]", program_name);
    std::println("Options:");
    std::println("  --version    Show version information");
    std::println("  --test-json  Test JSON parsing with simdjson");
    std::println("  --help       Show this help message");
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
    std::println("  simdjson: enabled");
#else
    std::println("  simdjson: disabled");
#endif
#ifdef ALPINE_BUILD
    std::println("  Build type: Alpine Linux (musl)");
#else
    std::println("  Build type: Standard Linux (glibc)");
#endif
}

#ifdef HAS_SIMDJSON
void test_json_parsing() {
    std::println("Testing simdjson parsing...");
    
    // Test JSON with custom raw string delimiter
    std::string json_data = R"json({
        "type": "execute",
        "language": "python",
        "code": "print('Hello, World!')",
        "timeout": 5000
    })json";
    
    try {
        simdjson::ondemand::parser parser;
        simdjson::padded_string padded(json_data);
        simdjson::ondemand::document doc = parser.iterate(padded);
        
        std::string_view type = doc["type"];
        std::string_view language = doc["language"];
        std::string_view code = doc["code"];
        int64_t timeout = doc["timeout"];
        
        std::println("Parsed successfully!");
        std::println("  Type: {}", type);
        std::println("  Language: {}", language);
        std::println("  Code: {}", code);
        std::println("  Timeout: {}ms", timeout);
        
        // Test performance
        auto start = std::chrono::high_resolution_clock::now();
        const int iterations = 100000;
        
        for (int i = 0; i < iterations; ++i) {
            simdjson::padded_string ps(json_data);
            auto d = parser.iterate(ps);
            // Force parsing
            [[maybe_unused]] auto t = d["type"];
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::println("\nPerformance test:");
        std::println("  Iterations: {}", iterations);
        std::println("  Total time: {} µs", duration.count());
        std::println("  Per iteration: {} µs", duration.count() / iterations);
        
    } catch (simdjson::simdjson_error& e) {
        std::println(stderr, "JSON parsing error: {}", e.what());
    }
}
#endif

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::println("vsocky - Firecracker VM code execution service");
        std::println("Starting in server mode...");
        
        // TODO: Implement VSock server
        std::println("VSock server not yet implemented.");
        return 0;
    }
    
    std::string arg(argv[1]);
    
    if (arg == "--version") {
        print_version();
        return 0;
    } else if (arg == "--help") {
        print_usage(argv[0]);
        return 0;
    } else if (arg == "--test-json") {
#ifdef HAS_SIMDJSON
        test_json_parsing();
#else
        std::println(stderr, "Error: vsocky was built without simdjson support");
        return 1;
#endif
        return 0;
    } else {
        std::println(stderr, "Unknown option: {}", arg);
        print_usage(argv[0]);
        return 1;
    }
}