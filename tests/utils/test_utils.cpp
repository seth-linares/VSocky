#include "vsocky/utils/error.hpp"
#include "vsocky/utils/base64.hpp"
#include "vsocky/utils/signal_handler.hpp"

#include <print>
#include <cassert>
#include <thread>
#include <chrono>


using namespace vsocky;

void test_error_codes() {
    std::println("Testing error codes...");
    
    // Make sure that we can properly convert between error codes and strings
    assert(error_to_string(error_code::success) == "success");
    assert(error_to_string(error_code::socket_creation_failed) == "socket creation failed");
    assert(error_to_string(error_code::invalid_json) == "invalid JSON");

    // Test to see if our custom error codes are properly converting to std::error_code
    // maybe_unused because the release compiler is optimizing it away since it doesn't count the asserts as reading ec
    [[maybe_unused]] auto ec = make_error_code(error_code::bind_failed); // specifically make sure that the compiler can deduce std::error_code
    assert(ec.category().name() == std::string("vsocky")); // remember -- category has virt funcs that we override .name() & .message()
    assert(ec.message() == "bind failed"); // error_code defines .message() as `.category().message()` so it's basically an alias
    

    std::println("✓ Error codes test passed\n");
}

void test_base64() {
    std::println("Testing base64 encoding/decoding...");

    // Test empty input
    {
        std::string empty;
        auto encoded = base64_encode(empty);
        assert(encoded.empty()); // make sure that it comes back empty

        auto decoded = base64_decode_string(encoded);
        assert(decoded.has_value()); // make sure that we don't throw an error/get std::unexpected()
        assert(decoded.value().empty());
    }

    // Test basic strings
    {
        std::string input = "Hello, World!";
        auto encoded = base64_encode(input);
        assert(encoded == "SGVsbG8sIFdvcmxkIQ==");
        
        auto decoded = base64_decode_string(encoded);
        assert(decoded.has_value()); 
        assert(decoded.value() == input);
    }

    // Test different padding cases
    {
        // No padding needed
        std::string input1 = "abc";
        assert(base64_encode(input1) == "YWJj");
        
        // One padding character
        std::string input2 = "abcd";
        assert(base64_encode(input2) == "YWJjZA==");
        
        // Two padding characters
        std::string input3 = "abcde";
        assert(base64_encode(input3) == "YWJjZGU=");
    }

    // Test binary data
    {
        std::vector<uint8_t> binary = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD};
        auto encoded = base64_encode(binary);

        auto decoded = base64_decode(encoded);
        assert(decoded.has_value());
        assert(decoded.value() == binary);
    }

    // Test invalid base64
    {
        auto result = base64_decode("Invalid@Base64!"); // decoding with chars that aren't allowed
        assert(!result.has_value());
        assert(result.error() == make_error_code(error_code::invalid_base64_encoding));
    }

    // Test malformed base64 (wrong length)
    {
        auto result = base64_decode("YWJ");  // Missing padding
        assert(!result.has_value());
    }

    // Test Python example from protocol
    {
        std::string code = "print('Hello, World!')";
        auto encoded = base64_encode(code);
        assert(encoded == "cHJpbnQoJ0hlbGxvLCBXb3JsZCEnKQ==");
    }


    std::println("✓ Base64 test passed\n");
}

void test_signal_handler() {
    std::println("Testing signal handler...");

    // Reset any previous state
    signal_handler::reset();
    assert(!signal_handler::should_shutdown()); // should be false

    // set up handlers
    signal_handler::setup();

    // still shouldn't want to shutdown
    assert(!signal_handler::should_shutdown());

    // Send ourselves SIGTERM
    std::println("\n  Sending SIGTERM...\n");
    raise(SIGTERM);

    // Give signal time to be delivered
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // should now be ready to shutdown
    assert(signal_handler::should_shutdown());

    std::println("✓ Signal handler test passed\n");
}

int main() {
    std::println("Running VSocky utility tests...\n");
    
    test_error_codes();
    test_base64();
    test_signal_handler();
    
    std::println("\nAll tests passed! ✓");
    return 0;
}