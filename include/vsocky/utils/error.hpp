#pragma once

#include <system_error>
#include <string_view>

namespace vsocky {
    /*
        For future reference:

        error_cateogry is essentially just utility for our error_codes. What do I mean? 
        The only thing we do with error category is override these 2 methods:

        `virtual const char* name() const noexcept = 0;`
        `virtual string message(int) const = 0;`

        name() gives us the actual category and basically just says "this error belongs to 'vsocky'!!"
        message() takes in the error code (as an int) and then gives us back the string version of it
            - we store those strings in `enum class error_code` and call `error_to_string()` to get the conversion

        error_code is the main class we deal with. Here is what it contains (that we care about):
            - Members: 
                - The error code (int)
                - A pointer to error_category
            - Methods: We use anything in std::error_code, but error_code.message() is just an alias for 
            error_category.message()!

        So essentially we interface everything through error_code but we can get access to category details via the
        pointer or aliased message()

    */

    // =============================================================================
    // STEP 1: Define Your Custom Error Codes
    // =============================================================================
    // This enum class defines all possible error conditions in your VSocky library.
    // Each error has a unique integer value (starting from 0 for success).
    // Using 'enum class' (not 'enum') provides type safety - you can't accidentally
    // mix these with regular integers without an explicit cast.
    enum class error_code {
        success = 0,
        
        // Socket errors
        socket_creation_failed,  // = 1 (implicit)
        bind_failed,            // = 2 (implicit)
        listen_failed,          // = 3 (implicit)
        accept_failed,          // = 4 (implicit)
        connection_closed,      // = 5 (implicit)
        read_failed,            // = 6 (implicit)
        write_failed,           // = 7 (implicit)
        
        // Protocol errors
        message_too_large,      // = 8 (implicit)
        invalid_message_format, // = 9 (implicit)
        invalid_json,          // = 10 (implicit)
        missing_required_field, // = 11 (implicit)
        invalid_field_value,    // = 12 (implicit)
        unsupported_message_type, // = 13 (implicit)
        unsupported_language,   // = 14 (implicit)
        
        // System errors
        resource_unavailable,   // = 15 (implicit)
        internal_error,         // = 16 (implicit)
        
        // Base64 errors
        invalid_base64_encoding, // = 17 (implicit)
        
        // General errors
        timeout,                // = 18 (implicit)
        interrupted             // = 19 (implicit)
    };

    // =============================================================================
    // STEP 2: Human-Readable Error Messages
    // =============================================================================
    // This function converts error codes to human-readable strings.
    // - constexpr: Can be evaluated at compile-time
    // - string_view: Lightweight, non-owning reference to string data
    // - noexcept: Promises this function won't throw exceptions
    constexpr std::string_view error_to_string(error_code ec) noexcept {
        switch (ec) {
            case error_code::success:
                return "success";
                
            // Socket errors
            case error_code::socket_creation_failed:
                return "socket creation failed";
            case error_code::bind_failed:
                return "bind failed";
            case error_code::listen_failed:
                return "listen failed";
            case error_code::accept_failed:
                return "accept failed";
            case error_code::connection_closed:
                return "connection closed";
            case error_code::read_failed:
                return "read failed";
            case error_code::write_failed:
                return "write failed";
                
            // Protocol errors
            case error_code::message_too_large:
                return "message too large";
            case error_code::invalid_message_format:
                return "invalid message format";
            case error_code::invalid_json:
                return "invalid JSON";
            case error_code::missing_required_field:
                return "missing required field";
            case error_code::invalid_field_value:
                return "invalid field value";
            case error_code::unsupported_message_type:
                return "unsupported message type";
            case error_code::unsupported_language:
                return "unsupported language";
                
            // System errors
            case error_code::resource_unavailable:
                return "resource unavailable";
            case error_code::internal_error:
                return "internal error";
                
            // Base64 errors
            case error_code::invalid_base64_encoding:
                return "invalid base64 encoding";
                
            // General errors
            case error_code::timeout:
                return "timeout";
            case error_code::interrupted:
                return "interrupted";
        }
        
        return "unknown error";
    }

    // =============================================================================
    // STEP 3: Error Category Implementation
    // =============================================================================
    // std::error_category is an abstract base class that provides:
    // 1. A name for your category of errors
    // 2. Human-readable messages for error codes
    // 3. Comparison logic between errors (we use the defaults)
    //
    // This allows std::error_code to know:
    // - Which "system" an error number belongs to (vsocky vs errno vs Windows)
    // - How to get a human-readable message for that error number
    class error_category_impl : public std::error_category {
    public:
        // Returns the name of this error category.
        // Used for debugging and in error messages like "vsocky:10"
        const char* name() const noexcept override {
            return "vsocky";
        }
        
        // Converts an error number to a human-readable message.
        // - ev: The error number (from your enum, cast to int)
        // - Returns: String description of the error
        //
        // This is called by std::error_code::message()
        std::string message(int ev) const override {
            // Reuse our error_to_string function, converting string_view to string
            return std::string(error_to_string(static_cast<error_code>(ev)));
        }
        
        // Note: We don't override default_error_condition() or equivalent()
        // The base class implementations are fine for our needs
    };

    // =============================================================================
    // STEP 4: Category Singleton
    // =============================================================================
    // Returns a reference to the single instance of our error category.
    // - inline: Allows definition in header without ODR violations
    // - static: The instance is created once and lives forever
    // - noexcept: This can't throw
    //
    // Why singleton? Categories are compared by address, so we need
    // exactly one instance that all error_codes can point to.
    inline const std::error_category& error_category() noexcept {
        static error_category_impl instance;
        return instance;
    }

    // =============================================================================
    // STEP 5: Bridge Function (ADL Hook)
    // =============================================================================
    // This function is THE BRIDGE between your enum and std::error_code.
    // 
    // CRITICAL: This MUST be in the same namespace as your error_code enum!
    // The compiler finds this via ADL (Argument-Dependent Lookup) when the
    // std::error_code template constructor is instantiated.
    //
    // What it does:
    // 1. Takes your enum value
    // 2. Converts it to int
    // 3. Pairs it with your category
    // 4. Returns a std::error_code containing both
    inline std::error_code make_error_code(error_code ec) noexcept {
        return {static_cast<int>(ec), error_category()};
        //      ^^^^^^^^^^^^^^^^^^    ^^^^^^^^^^^^^^^^^
        //      Your error number     Your category singleton
    }
} // namespace vsocky

// =============================================================================
// STEP 6: Template Specialization (The Magic Enable Switch)
// =============================================================================
// This is where we tell the C++ standard library: "Yes, vsocky::error_code is
// a valid error code enum that should work with automatic conversion."
//
// In essence we are actually modifying the std lib but the reason this is fine
// is because we are just saying "hey, vsocky::error_code is an error type!".
// 
// MUST be in namespace std (we're specializing a standard library template)
namespace std {
    // template<> means FULL SPECIALIZATION - we're providing a concrete type
    // (vsocky::error_code) for the template parameter, so no parameters remain.
    //
    // By inheriting from true_type (instead of the default false_type), we set
    // is_error_code_enum<vsocky::error_code>::value = true
    //
    // This enables the std::error_code template constructor that accepts our enum!
    template<>
    struct is_error_code_enum<vsocky::error_code> : true_type {};
} // namespace std

// =============================================================================
// HOW IT ALL WORKS TOGETHER
// =============================================================================
// When you write:
//     std::error_code ec = vsocky::error_code::invalid_json;
//
// 1. Compiler looks for a constructor of std::error_code that accepts your enum
// 2. Finds the template constructor, but it's conditionally enabled
// 3. Checks: is_error_code_enum<vsocky::error_code>::value
// 4. Our specialization makes this true, so constructor is enabled
// 5. Constructor calls make_error_code(vsocky::error_code::invalid_json)
// 6. ADL finds vsocky::make_error_code because the argument is from vsocky::
// 7. make_error_code returns std::error_code{10, &vsocky_category}
// 8. ec now contains: { _M_value: 10, _M_cat: pointer-to-vsocky-category }
//
// =============================================================================
// USAGE EXAMPLES
// =============================================================================
// // Direct construction (automatic, thanks to our specialization)
// std::error_code ec1 = vsocky::error_code::socket_creation_failed;
//
// // Check for error
// if (ec1) {  // operator bool() returns true if error (non-zero value)
//     std::cerr << "Error: " << ec1.message() << "\n";
//     std::cerr << "Category: " << ec1.category().name() << "\n";
//     std::cerr << "Value: " << ec1.value() << "\n";
// }
//
// // Use with system_error exception
// throw std::system_error(ec1, "Failed to initialize socket");
//
// // Return from functions
// std::error_code do_something() {
//     if (failed) {
//         return vsocky::error_code::resource_unavailable;
//     }
//     return vsocky::error_code::success;
// }
// =============================================================================