#include "Archive.h"

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

        in.read((char*)&e.originalSize, sizeof(e.originalSize));
        in.read((char*)&e.dataOffset, sizeof(e.dataOffset));
        in.read((char*)&e.crc32, sizeof(e.crc32));
    }

    model.data.clear();

    uint64_t currentOffset = 0;

    for (auto& e : model.index)
    {
        std::vector<uint8_t> buffer(e.originalSize);

        in.seekg(e.dataOffset);

        in.read(
            (char*)buffer.data(),
            e.originalSize);

        e.dataOffset = currentOffset;

        model.data.insert(
            model.data.end(),
            buffer.begin(),
            buffer.end());
        currentOffset += e.originalSize;
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
            sizeof(uint64_t) +
            sizeof(uint64_t) +
            sizeof(uint32_t);
    }

    uint64_t currentOffset = headerSize;

    std::vector<FileEntry> updatedIndex =
        model.index;

    for (auto& e : updatedIndex)
    {
        uint64_t archiveOffset = currentOffset;
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
        out.write((char*)&e.dataOffset, sizeof(e.dataOffset));
        out.write((char*)&e.crc32, sizeof(e.crc32));
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
        std::vector<uint8_t> data;
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

        TempFile f;
        f.name = std::filesystem::path(path).generic_string(); // 👈 ПАПКИ СОХРАНЯЕМ
        f.data.resize(size);

        in.read((char*)f.data.data(), size);

        tempFiles.push_back(std::move(f));
    }

    // ---------- BUILD MODEL ----------
    uint64_t offset = 0;

    for (auto& f : tempFiles)
    {
        FileEntry e;
        e.name = f.name;
        e.originalSize = f.data.size();
        e.dataOffset = offset;
        e.crc32 = crc32(f.data.data(), f.data.size());

        model.index.push_back(e);

        model.data.insert(
            model.data.end(),
            f.data.begin(),
            f.data.end());

        offset += f.data.size();
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

    for (const auto& path : files)
    {
        std::ifstream f(path, std::ios::binary);

        if (!f)
            return false;

        f.seekg(0, std::ios::end);
        size_t size = f.tellg();
        f.seekg(0);

        std::vector<uint8_t> buffer(size);

        f.read( 
            (char*)buffer.data(), 
            size); 
        
        uint64_t offset = model.data.size(); 
        model.data.insert(
            model.data.end(),
            buffer.begin(),
            buffer.end());

        FileEntry e;
        e.name = std::filesystem::path(path).generic_string();
        e.originalSize = size;
        e.compressedSize = size;
        e.dataOffset = model.data.size();
        e.crc32 =crc32(buffer.data(),size);

        model.index.push_back(e);
    }

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

        out.write(
            (char*)model.data.data() + f.dataOffset,
            f.originalSize);
    }

    return true;
}

bool Archive::info(const std::string& archiveName)
{
    std::ifstream in(archiveName, std::ios::binary);

    if (!in)
    {
        std::cerr << "Cannot open archive\n";
        return false;
    }

    ArchiveModel model = loadArchive(in);

    uint64_t totalSize = 0;

    std::cout << "Archive v2 info\n\n";
    std::cout << "Files: " << model.index.size() << "\n\n";

    uint64_t total = 0;

    for (const auto& f : model.index)
    {
        std::cout << f.name << "\n";
        std::cout << "  size: " << f.originalSize << "\n";
        std::cout << "  crc : " << f.crc32 << "\n\n";

        total += f.originalSize;
    }

    std::cout << "Total: " << total << "\n";

    std::cout << "Compressed size: "
              << model.data.size()
              << " bytes\n";

    return true;
}
