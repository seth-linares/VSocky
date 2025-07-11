#include <iostream>
#include <string>
#include <string_view>
#include <cstdlib>
#include <chrono>

#ifdef HAS_SIMDJSON
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wundef"
#include <simdjson.h>
#pragma GCC diagnostic pop
#endif

// Version info
constexpr const char* VSOCKY_VERSION = "0.1.0";

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  --version    Show version information\n"
              << "  --test-json  Test JSON parsing with simdjson\n"
              << "  --help       Show this help message\n";
}

void print_version() {
    std::cout << "vsocky version " << VSOCKY_VERSION << "\n";
    std::cout << "Build info:\n";
#ifdef __VERSION__
    std::cout << "  Compiler: " << __VERSION__ << "\n";
#else
    std::cout << "  Compiler: Unknown\n";
#endif
#ifdef HAS_SIMDJSON
    std::cout << "  simdjson: enabled\n";
#else
    std::cout << "  simdjson: disabled\n";
#endif
#ifdef ALPINE_BUILD
    std::cout << "  Build type: Alpine Linux (musl)\n";
#else
    std::cout << "  Build type: Standard Linux (glibc)\n";
#endif
}

#ifdef HAS_SIMDJSON
void test_json_parsing() {
    std::cout << "Testing simdjson parsing...\n";
    
    // Test JSON || custom raw string delimiter -> R"json()json" which is required!!
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
        
        std::cout << "Parsed successfully!\n";
        std::cout << "  Type: " << type << "\n";
        std::cout << "  Language: " << language << "\n";
        std::cout << "  Code: " << code << "\n";
        std::cout << "  Timeout: " << timeout << "ms\n";
        
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
        
        std::cout << "\nPerformance test:\n";
        std::cout << "  Iterations: " << iterations << "\n";
        std::cout << "  Total time: " << duration.count() << " µs\n";
        std::cout << "  Per iteration: " << (duration.count() / iterations) << " µs\n";
        
    } catch (simdjson::simdjson_error& e) {
        std::cerr << "JSON parsing error: " << e.what() << "\n";
    }
}
#endif

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "vsocky - Firecracker VM code execution service\n";
        std::cout << "Starting in server mode...\n";
        
        // TODO: Implement VSock server
        std::cout << "VSock server not yet implemented.\n";
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
        std::cerr << "Error: vsocky was built without simdjson support\n";
        return 1;
#endif
        return 0;
    } else {
        std::cerr << "Unknown option: " << arg << "\n";
        print_usage(argv[0]);
        return 1;
    }
}