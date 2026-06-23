#pragma once

#include <array>
#include <cstdint>
#include <vector>

class Huffman
{
public:

    static std::vector<uint8_t> compress(
        const std::vector<uint8_t>& input,
        std::array<uint64_t,256>& frequencies,
        uint32_t& outBitSize);

    static std::vector<uint8_t> decompress(
        const std::vector<uint8_t>& compressed,
        const std::array<uint64_t,256>& frequencies,
        uint64_t originalSize,
        uint32_t bitSize);
};