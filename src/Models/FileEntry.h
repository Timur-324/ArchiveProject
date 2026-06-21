//для включения файла в компиляцию только один раз
#pragma once

#include <string>
#include <cstdint>
struct FileEntry
{
    std::string name;
    uint64_t originalSize = 0;
    uint64_t compressedSize = 0;
    uint64_t dataOffset = 0;
    uint32_t crc32 = 0;
};