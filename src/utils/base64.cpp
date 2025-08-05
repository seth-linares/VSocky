#include "vsocky/utils/base64.hpp"
#include "vsocky/utils/error.hpp"

#include <array>

// =============================================================================
// BASE64 ENCODING EXPLAINED
// =============================================================================
// Base64 converts binary data to text using only 64 "safe" ASCII characters:
// A-Z, a-z, 0-9, +, / (and = for padding)
//
// THE PROBLEM:
// - Binary data uses all 256 possible byte values (0-255)
// - Many systems can't handle all byte values (null bytes, control chars, etc.)
// - Email, JSON, XML need text-only data
//
// THE SOLUTION:
// - Use only 64 characters that are safe everywhere
// - 64 = 2^6, so each character represents 6 bits
// - But our data is in 8-bit bytes...
//
// THE MATH:
// - Input: 8 bits per byte
// - Output: 6 bits per character
// - LCM(8,6) = 24 bits
// - So we process 3 bytes (24 bits) → 4 characters (24 bits)
//
// EXAMPLE: Encoding "Man"
// ASCII values: M=77, a=97, n=110
// Binary: 01001101 01100001 01101110
// Split into 6-bit groups: 010011|010110|000101|101110
// Decimal: 19, 22, 5, 46
// Base64: T, W, F, u
// Result: "TWFu"
// =============================================================================

namespace vsocky {

namespace {

// Base64 encoding table - maps 6-bit values (0-63) to ASCII characters
constexpr std::array<char, 64> encode_table = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',  // 0-7
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',  // 8-15
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',  // 16-23
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',  // 24-31
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',  // 32-39
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',  // 40-47
    'w', 'x', 'y', 'z', '0', '1', '2', '3',  // 48-55
    '4', '5', '6', '7', '8', '9', '+', '/'   // 56-63
};

// Build reverse lookup table at compile time
// This lets us decode by looking up decode_table['A'] = 0, etc.
constexpr std::array<int8_t, 256> create_decode_table() {
    std::array<int8_t, 256> table{};
    
    // Initialize all to -1 (invalid character marker)
    for (auto& val : table) {
        val = -1;
    }
    
    // For each valid base64 character, store its 6-bit value
    // Example: 'A' is at index 0, so decode_table['A'] = 0
    for (size_t i = 0; i < encode_table.size(); ++i) {
        table[static_cast<unsigned char>(encode_table[i])] = static_cast<int8_t>(i);
    }
    
    // Special marker for padding character
    table[static_cast<unsigned char>('=')] = -2;
    
    return table;
}

constexpr auto decode_table = create_decode_table();

} // anonymous namespace

std::string base64_encode(std::span<const uint8_t> data) {
    if (data.empty()) {
        return {};
    }
    
    // Calculate output size
    // Every 3 input bytes become 4 output characters
    // If input size isn't divisible by 3, we'll add padding
    const size_t output_size = ((data.size() + 2) / 3) * 4;
    std::string result;
    result.reserve(output_size);
    
    size_t i = 0;
    
    // Process complete 3-byte groups
    for (; i + 2 < data.size(); i += 3) {
        // =====================================================================
        // BIT MANIPULATION EXPLAINED
        // =====================================================================
        // We have 3 bytes (24 bits) that we need to split into 4 6-bit values
        //
        // Example with bytes: [0xFC, 0x0F, 0x03]
        // Binary: 11111100 00001111 00000011
        //
        // Step 1: Combine into single 24-bit value
        // data[i] << 16:   11111100 00000000 00000000  (shift left 16 bits)
        // data[i+1] << 8:  00000000 00001111 00000000  (shift left 8 bits)  
        // data[i+2]:       00000000 00000000 00000011  (no shift)
        // OR together:     11111100 00001111 00000011  (24-bit value)
        // =====================================================================
        const uint32_t triple = (static_cast<uint32_t>(data[i]) << 16) |
                               (static_cast<uint32_t>(data[i + 1]) << 8) |
                                static_cast<uint32_t>(data[i + 2]);
        
        // =====================================================================
        // EXTRACTING 6-BIT VALUES
        // =====================================================================
        // Now we extract four 6-bit values from our 24-bit triple
        //
        // Continuing example: triple = 11111100 00001111 00000011
        //
        // First 6 bits (bits 23-18):
        // triple >> 18:    00000000 00000000 00111111  (shift right 18)
        // & 0x3F:          00000000 00000000 00111111  (mask lower 6 bits)
        // Result: 63 → encode_table[63] = '/'
        //
        // Next 6 bits (bits 17-12):
        // triple >> 12:    00000000 00001111 11000000  (shift right 12)
        // & 0x3F:          00000000 00000000 00000000  (mask lower 6 bits)
        // Result: 0 → encode_table[0] = 'A'
        //
        // 0x3F in binary is 00111111 - it masks out all but the lower 6 bits
        // =====================================================================
        result.push_back(encode_table[(triple >> 18) & 0x3F]);  // Bits 23-18
        result.push_back(encode_table[(triple >> 12) & 0x3F]);  // Bits 17-12
        result.push_back(encode_table[(triple >> 6) & 0x3F]);   // Bits 11-6
        result.push_back(encode_table[triple & 0x3F]);          // Bits 5-0
    }
    
    // =========================================================================
    // HANDLING REMAINING BYTES (PADDING)
    // =========================================================================
    // If input length % 3 != 0, we have 1 or 2 leftover bytes
    // Base64 always outputs groups of 4 characters, so we pad with '='
    //
    // 1 leftover byte:
    //   - Encodes to 2 characters (12 bits: 8 data + 4 padding)
    //   - Add two '=' padding characters
    //   Example: 'M' → "TQ=="
    //
    // 2 leftover bytes:
    //   - Encode to 3 characters (18 bits: 16 data + 2 padding)
    //   - Add one '=' padding character
    //   Example: 'Ma' → "TWE="
    // =========================================================================
    if (i < data.size()) {
        // Start with first leftover byte shifted to high position
        uint32_t triple = static_cast<uint32_t>(data[i]) << 16;
        
        // Add second leftover byte if present
        if (i + 1 < data.size()) {
            triple |= static_cast<uint32_t>(data[i + 1]) << 8;
        }
        
        // Always encode first two characters (we have at least 1 byte)
        result.push_back(encode_table[(triple >> 18) & 0x3F]);
        result.push_back(encode_table[(triple >> 12) & 0x3F]);
        
        // If we had 2 bytes, encode third character; otherwise pad
        if (i + 1 < data.size()) {
            result.push_back(encode_table[(triple >> 6) & 0x3F]);
            result.push_back('=');  // Only one padding needed
        } else {
            result.push_back('=');  // Two padding needed
            result.push_back('=');
        }
    }
    
    return result;
}

std::expected<std::vector<uint8_t>, std::error_code> 
base64_decode(std::string_view encoded) {
    if (encoded.empty()) {
        return std::vector<uint8_t>{};
    }
    
    // Count padding to calculate actual data size
    size_t padding = 0;
    if (encoded.size() >= 2 && encoded[encoded.size() - 1] == '=') {
        padding++;
        if (encoded[encoded.size() - 2] == '=') {
            padding++;
        }
    }
    
    // Base64 must be multiple of 4 characters
    if (encoded.size() % 4 != 0) {
        return std::unexpected(make_error_code(error_code::invalid_base64_encoding));
    }
    
    // Calculate output size
    // 4 characters decode to 3 bytes, minus padding
    const size_t output_size = (encoded.size() / 4) * 3 - padding;
    std::vector<uint8_t> result;
    result.reserve(output_size);
    
    // Process 4 characters at a time
    for (size_t i = 0; i < encoded.size(); i += 4) {
        // Decode 4 characters to their 6-bit values
        std::array<int8_t, 4> values{};
        
        for (size_t j = 0; j < 4; ++j) {
            const auto ch = static_cast<unsigned char>(encoded[i + j]);
            values[j] = decode_table[ch];
            
            // Check for invalid character
            if (values[j] == -1) {
                return std::unexpected(make_error_code(error_code::invalid_base64_encoding));
            }
            
            // Padding should only appear at the end
            if (values[j] == -2 && i + j < encoded.size() - padding) {
                return std::unexpected(make_error_code(error_code::invalid_base64_encoding));
            }
        }
        
        // =====================================================================
        // RECONSTRUCTING BYTES FROM 6-BIT VALUES
        // =====================================================================
        // We have four 6-bit values that we combine back into 3 bytes
        // This is the reverse of encoding: combine 4x6 bits → 3x8 bits
        //
        // Example: ['T', 'W', 'F', 'u'] → [19, 22, 5, 46] → "Man"
        // Binary: 010011 010110 000101 101110
        // Combine: 01001101 01100001 01101110
        // Result: [77, 97, 110] = "Man"
        // =====================================================================
        const uint32_t triple = 
            (static_cast<uint32_t>(values[0] & 0x3F) << 18) |  // Bits 23-18
            (static_cast<uint32_t>(values[1] & 0x3F) << 12) |  // Bits 17-12
            (static_cast<uint32_t>((values[2] == -2 ? 0 : values[2]) & 0x3F) << 6) |  // Bits 11-6
             static_cast<uint32_t>((values[3] == -2 ? 0 : values[3]) & 0x3F);          // Bits 5-0
        
        // Extract 3 bytes from our 24-bit value
        result.push_back(static_cast<uint8_t>((triple >> 16) & 0xFF));  // Byte 1
        
        // Only add byte 2 if no padding in position 3
        if (values[2] != -2) {
            result.push_back(static_cast<uint8_t>((triple >> 8) & 0xFF));  // Byte 2
        }
        
        // Only add byte 3 if no padding in position 4
        if (values[3] != -2) {
            result.push_back(static_cast<uint8_t>(triple & 0xFF));  // Byte 3
        }
    }
    
    return result;
}

std::expected<std::string, std::error_code>
base64_decode_string(std::string_view encoded) {
    auto decoded = base64_decode(encoded);
    if (!decoded) {
        return std::unexpected(decoded.error());
    }
    
    // Construct string from vector of bytes
    // string constructor takes iterators to beginning and end of range
    return std::string(decoded->begin(), decoded->end());
}

} // namespace vsocky