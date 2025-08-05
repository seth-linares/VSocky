#pragma once

#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <cstdint>

// =============================================================================
// UNDERSTANDING std::span (C++20)
// =============================================================================
// std::span is a lightweight view over a contiguous sequence of objects.
// Think of it as a "pointer + size" pair that's safer than raw pointers.
//
// COMPARED TO C# SPAN:
// Very similar concept! Both provide:
// - Zero-copy view into existing memory
// - Bounds checking in debug builds
// - No ownership (doesn't manage memory lifetime)
//
// WHY USE span INSTEAD OF POINTER + SIZE?
// 1. Type safety: Can't accidentally swap pointer and size arguments
// 2. Convenient: Has begin(), end(), size() methods
// 3. Debug safety: Can add bounds checking
// 4. Clear intent: Shows we want to read/write existing memory
// 5. Works with any contiguous container (vector, array, C-array)
//
// Example:
//   void process(uint8_t* data, size_t size);  // Old C style
//   void process(std::span<uint8_t> data);     // Modern C++ style
// =============================================================================

namespace vsocky {

// Base64 encode binary data
std::string base64_encode(std::span<const uint8_t> data);

// =============================================================================
// UNDERSTANDING THE inline KEYWORD
// =============================================================================
// In C++, 'inline' has evolved from its original meaning:
//
// HISTORICAL MEANING (C++98):
// "Compiler, please try to inline this function" (replace call with body)
//
// MODERN MEANING (C++17+):
// "This function can be defined in a header without causing linker errors"
//
// THE ONE DEFINITION RULE (ODR):
// Normally, if you define a function in a header and include it in multiple
// .cpp files, the linker sees multiple definitions and fails.
//
// 'inline' tells the linker: "All definitions are identical, just pick one"
//
// WHY WE USE IT HERE:
// This function is defined in the header (not just declared), so we need
// 'inline' to avoid linker errors when multiple .cpp files include this.
//
// WHEN TO USE inline:
// 1. Function definitions in headers (like here)
// 2. Small functions you want in headers for performance
// 3. Template specializations in headers
// =============================================================================
inline std::string base64_encode(std::string_view str) {
    // =========================================================================
    // UNDERSTANDING reinterpret_cast
    // =========================================================================
    // C++ has 4 types of casts (unlike C's single cast):
    // 1. static_cast: Safe conversions (int to double, Derived* to Base*)
    // 2. dynamic_cast: Runtime-checked conversions (Base* to Derived*)
    // 3. const_cast: Add/remove const (dangerous!)
    // 4. reinterpret_cast: Reinterpret bit pattern as different type
    //
    // reinterpret_cast is the most dangerous - it says "trust me, treat
    // these bits as this other type". No safety checks!
    //
    // HERE'S WHAT'S HAPPENING:
    // - str.data() returns const char*
    // - We need const uint8_t* for base64_encode
    // - char and uint8_t have same size (1 byte) but different types
    // - reinterpret_cast says "treat these char bytes as uint8_t bytes"
    //
    // WHY NOT static_cast?
    // static_cast can't convert between unrelated pointer types.
    // char* and uint8_t* are considered unrelated by C++ type system.
    //
    // IS THIS SAFE?
    // Yes, because:
    // 1. char and uint8_t have same size and alignment
    // 2. We're just viewing the same bytes differently
    // 3. Base64 works on raw bytes regardless of interpretation
    // =========================================================================
    
    // =========================================================================
    // UNDERSTANDING std::string::data()
    // =========================================================================
    // std::string stores characters in a contiguous array internally.
    // 
    // data() returns a pointer to this internal array:
    // - Returns: const char* (can't modify through this pointer)
    // - Guaranteed: Points to size() characters
    // - Null terminator: Present (as of C++11) but not included in size()
    // - Lifetime: Valid until string is modified or destroyed
    //
    // Similar methods:
    // - c_str(): Same as data() in C++11+ (historically different)
    // - &str[0]: Works for non-const strings, UB if string is empty
    //
    // Example memory layout for string "Hi!":
    // [H][i][!][\0]
    //  ^
    //  data() points here
    // =========================================================================
    return base64_encode(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(str.data()), str.size()));
}

// Base64 decode to binary data
std::expected<std::vector<uint8_t>, std::error_code> 
base64_decode(std::string_view encoded);

// Base64 decode to string
std::expected<std::string, std::error_code>
base64_decode_string(std::string_view encoded);

} // namespace vsocky