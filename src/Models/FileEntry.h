//для включения файла в компиляцию только один раз
#pragma once

#include <string>
#include <cstdint>

struct FileEntry
{
    std::string name; // теперь МОЖЕТ быть folder/file.txt
    uint64_t size = 0;
    uint64_t offset = 0;
    uint32_t crc32 = 0;
};