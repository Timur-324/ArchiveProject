//для включения файла в компиляцию только один раз
#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <vector>

struct HuffmanNodeInfo
{
    uint8_t symbol;
    uint32_t frequency;
};

struct FileEntry
{
    std::string name;
    uint64_t originalSize = 0;
    uint64_t compressedSize = 0;
    uint64_t dataOffset = 0;
    uint32_t crc32 = 0;
    uint32_t bitSize = 0;
    std::vector<HuffmanNodeInfo> frequencies;
};