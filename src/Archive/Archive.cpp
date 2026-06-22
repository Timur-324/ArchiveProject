#include "Archive.h"
#include "Huffman.h"
#include <filesystem>
#include <iostream>
#include <vector>

constexpr uint32_t MAGIC   = 0x48415243; // "HARC"
constexpr uint32_t VERSION = 2;

uint32_t crc32(const uint8_t* data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < len; i++)
    {
        crc ^= data[i];

        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }

    return ~crc;
}

struct MemoryFile
{
    std::string name;
    std::vector<uint8_t> data;
};

struct ArchiveModel
{
    std::vector<FileEntry> index;
    std::vector<uint8_t> data;
};

ArchiveModel loadArchive(std::ifstream& in)
{
    ArchiveModel model;

    uint32_t magic;
    uint32_t version;
    uint32_t count;

    in.read((char*)&magic, sizeof(magic));
    in.read((char*)&version, sizeof(version));
    in.read((char*)&count, sizeof(count));

    if (magic != MAGIC)
        throw std::runtime_error("Invalid archive");

    if (version != VERSION)
        throw std::runtime_error("Unsupported archive version");

    model.index.resize(count);

    for (auto& e : model.index)
    {
        uint32_t nameLen;

        in.read((char*)&nameLen, sizeof(nameLen));

        e.name.resize(nameLen);
        in.read(e.name.data(), nameLen);

        in.read((char*)&e.originalSize,sizeof(e.originalSize));
        in.read((char*)&e.compressedSize,sizeof(e.compressedSize));
        in.read((char*)&e.dataOffset,sizeof(e.dataOffset));
        in.read((char*)&e.crc32,sizeof(e.crc32));

        for (auto& freq : e.frequencies)
        {
            in.read(
                (char*)&freq,
                sizeof(freq));
        }
    }

    model.data.clear();

    uint64_t currentOffset = 0;

    for (auto& e : model.index)
    {
        std::vector<uint8_t> buffer(e.compressedSize);

        in.seekg(e.dataOffset);

        in.read(
            (char*)buffer.data(),
            e.compressedSize);

        e.dataOffset = currentOffset;

        model.data.insert(
            model.data.end(),
            buffer.begin(),
            buffer.end());
        currentOffset += e.compressedSize;
    }

    return model;
}

bool writeArchive(
    const std::string& archiveName,
    const ArchiveModel& model)
{
    std::ofstream out(
        archiveName,
        std::ios::binary);

    if (!out)
        return false;

    uint32_t count =
        static_cast<uint32_t>(
            model.index.size());

    uint64_t headerSize =
        sizeof(MAGIC) +
        sizeof(VERSION) +
        sizeof(count);

    for (const auto& e : model.index)
    {
        headerSize +=
            sizeof(uint32_t) +
            e.name.size() +
            sizeof(uint64_t) + // originalSize
            sizeof(uint64_t) + // compressedSize
            sizeof(uint64_t) + // dataOffset
            sizeof(uint32_t) + // crc32
            sizeof(uint64_t) * 256; // frequencies
    }

    uint64_t currentOffset = headerSize;

    std::vector<FileEntry> updatedIndex =
        model.index;

    for (auto& e : updatedIndex)
    {
        e.dataOffset = currentOffset;
        currentOffset += e.compressedSize;
    }


    out.write((char*)&MAGIC, sizeof(MAGIC));
    out.write((char*)&VERSION, sizeof(VERSION));
    out.write((char*)&count, sizeof(count));

    for (const auto& e : updatedIndex)
    {
        uint32_t len =
            static_cast<uint32_t>(
                e.name.size());

        out.write((char*)&len, sizeof(len));
        out.write(e.name.data(), len);

        out.write((char*)&e.originalSize, sizeof(e.originalSize));
        out.write((char*)&e.compressedSize, sizeof(e.compressedSize));
        out.write((char*)&e.dataOffset, sizeof(e.dataOffset));
        out.write((char*)&e.crc32, sizeof(e.crc32));

        for (uint64_t freq : e.frequencies)
        {
            out.write(
                (char*)&freq,
                sizeof(freq));
        }
    }

    out.write(
        (char*)model.data.data(),
        model.data.size());

    return true;
}

bool Archive::create(
    const std::string& archiveName,
    const std::vector<std::string>& files)
{
    ArchiveModel model;

    struct TempFile
    {
         std::string name;

        std::vector<uint8_t> buffer;        // оригинальные данные
        std::vector<uint8_t> compressed;    // сжатые данные

        std::array<uint64_t, 256> frequencies;

        size_t originalSize;
        size_t compressedSize;
    };

    std::vector<TempFile> tempFiles;

    // ---------- LOAD FILES ----------
    for (const auto& path : files)
    {
        std::cout << "Trying: " << path << '\n';

        std::ifstream in(path, std::ios::binary);

        if (!in)
        {
            std::cerr
                << "Cannot open file: "
                << path
                << '\n';

            return false;
        }

        in.seekg(0, std::ios::end);
        size_t size = in.tellg();
        in.seekg(0);

        std::vector<uint8_t> buffer(size);

        in.read(
            (char*)buffer.data(),
            size);

        std::array<uint64_t,256> frequencies;

        auto compressed =
            Huffman::compress(
                buffer,
                frequencies);

        TempFile f;

        f.name = std::filesystem::path(path).generic_string();

        f.buffer = std::move(buffer);
        f.compressed = std::move(compressed);
        f.frequencies = frequencies;

        f.originalSize = f.buffer.size();
        f.compressedSize = f.compressed.size();

        tempFiles.push_back(std::move(f));
    }

    // ---------- BUILD MODEL ----------
    uint64_t offset = 0;

    for (auto& f : tempFiles)
    {
        FileEntry e;

        e.name = f.name;

        // размеры
        e.originalSize = f.buffer.size();
        e.compressedSize = f.compressed.size();

        // Huffman данные
        e.frequencies = f.frequencies;

        e.dataOffset = offset;

        // CRC по оригиналу
        e.crc32 =
            crc32(
                f.buffer.data(),
                f.buffer.size());

        model.index.push_back(e);

        // В архив пишем ТОЛЬКО compressed
        model.data.insert(
            model.data.end(),
            f.compressed.begin(),
            f.compressed.end());

        offset += f.compressed.size();
    }
    
    // ---------- WRITE ----------
    return writeArchive(
        archiveName,
        model);
}

bool Archive::add(
    const std::string& archiveName,
    const std::vector<std::string>& files)
{
    std::ifstream in(archiveName, std::ios::binary);

    if (!in)
        return false;

    ArchiveModel model = loadArchive(in);
    in.close();

    for (const auto& path : files)
    {
        std::ifstream f(path, std::ios::binary);

        if (!f)
            return false;

        f.seekg(0, std::ios::end);
        size_t size = f.tellg();
        f.seekg(0);

        std::vector<uint8_t> buffer(size);

        f.read((char*)buffer.data(), size);

        std::array<uint64_t, 256> frequencies{};

        auto compressed =
            Huffman::compress(buffer, frequencies);

        FileEntry e;

        e.name = std::filesystem::path(path).generic_string();

        e.originalSize = buffer.size();
        e.compressedSize = compressed.size();
        e.frequencies = frequencies;

        e.crc32 = crc32(
            buffer.data(),
            buffer.size());

        e.dataOffset = model.data.size();

        model.index.push_back(e);

        model.data.insert(
            model.data.end(),
            compressed.begin(),
            compressed.end());
    }

    std::cout << "INDEX SIZE: " << model.index.size() << "\n";
    std::cout << "DATA SIZE: " << model.data.size() << "\n";

    return writeArchive(archiveName, model);
}

bool Archive::remove(
    const std::string& archiveName,
    const std::string& fileName)
{
    std::ifstream in(archiveName, std::ios::binary);

    if (!in)
        return false;

    ArchiveModel model = loadArchive(in);

    ArchiveModel newModel;

    uint64_t offset = 0;

    for (const auto& e : model.index)
    {
       bool match =
        e.name == fileName ||
        std::filesystem::path(e.name).filename()
            ==
        std::filesystem::path(fileName).filename();

        if (match)
        {
            continue;
        }

        FileEntry ne = e;
        ne.dataOffset = offset;

        newModel.index.push_back(ne);

        newModel.data.insert(
            newModel.data.end(),
            model.data.begin() + e.dataOffset,
            model.data.begin() + e.dataOffset + e.originalSize);

        offset += e.originalSize;
    }

    return writeArchive(archiveName, newModel);
}

bool Archive::list(const std::string& archiveName)
{
    std::ifstream in(archiveName, std::ios::binary);

    if (!in)
    {
        std::cerr << "Cannot open archive\n";
        return false;
    }

    ArchiveModel model = loadArchive(in);

    std::cout << "Files in archive (v2):\n\n";

    for (const auto& f : model.index)
    {
        std::cout
            << f.name
            << " ("
            << f.originalSize
            << " bytes, crc="
            << f.crc32
            << ")\n";
    }

    return true;
}


bool Archive::extract(
    const std::string& archiveName,
    const std::string& outputDir)
{
    std::ifstream in(archiveName, std::ios::binary);

    if (!in)
    {
        std::cerr << "Cannot open archive\n";
        return false;
    }

    ArchiveModel model = loadArchive(in);

    std::filesystem::create_directories(outputDir);

    for (const auto& f : model.index)
    {
        std::filesystem::path outPath =
            std::filesystem::path(outputDir) / f.name;

        // ВАЖНО: создаём вложенные папки
        std::filesystem::create_directories(
            outPath.parent_path());

        std::ofstream out(
            outPath,
            std::ios::binary);

        if (!out)
        {
            std::cerr << "Cannot create file: "
                      << outPath << '\n';
            continue;
        }

        std::vector<uint8_t> compressed(
            model.data.begin() + f.dataOffset,
            model.data.begin() + f.dataOffset + f.compressedSize);

        auto decompressed =
            Huffman::decompress(
                compressed,
                f.frequencies,
                f.originalSize);

        out.write(
            (char*)decompressed.data(),
            decompressed.size());
    }

    return true;
}

bool Archive::info(
    const std::string& archiveName)
{
    std::ifstream in(archiveName, std::ios::binary);

    if (!in)
    {
        std::cerr << "Cannot open archive\n";
        return false;
    }

    ArchiveModel model = loadArchive(in);

    std::cout << "Archive v2 info\n\n";
    std::cout << "Files: " << model.index.size() << "\n\n";

    uint64_t totalOriginal = 0;
    uint64_t totalCompressed = 0;

    for (const auto& e : model.index)
    {
        std::cout << e.name << "\n";
        std::cout << "  original size  : " << e.originalSize << "\n";
        std::cout << "  compressed size: " << e.compressedSize << "\n";
        std::cout << "  crc            : " << e.crc32 << "\n\n";

        totalOriginal += e.originalSize;
        totalCompressed += e.compressedSize;
    }

    std::cout << "Total original size: " << totalOriginal << " bytes\n";
    std::cout << "Total compressed size: " << totalCompressed << " bytes\n";

    double ratio = 0.0;

    if (totalOriginal != 0)
    {
        ratio =
            100.0 *
            (1.0 -
            (double)totalCompressed /
            (double)totalOriginal);
    }

    std::cout << "Compression: " << ratio << "%\n";

    return true;
}
