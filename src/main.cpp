#include "Archive.h"
#include "Huffman.h"
#include <iostream>


void testingHuffman()
{
    std::string text =
        "AAAAAAABBBBBBBBBCCCCCCCCCCCC";

    std::vector<uint8_t> data(
        text.begin(),
        text.end());

    std::array<uint64_t,256> frequencies;

    auto compressed =
        Huffman::compress(
            data,
            frequencies);

    auto decompressed =
        Huffman::decompress(
            compressed,
            frequencies,
            data.size());

    std::string restored(
        decompressed.begin(),
        decompressed.end());

    std::cout
        << "Original size: "
        << data.size()
        << " bytes\n";

    std::cout
        << "Compressed size: "
        << compressed.size()
        << " bytes\n";

    std::cout
        << "Restored text: "
        << restored
        << '\n';

    std::cout
        << "Equal: "
        << (restored == text ? "YES" : "NO")
        << '\n';
}

int main(int argc, char* argv[])
{

    // testingHuffman();
    Archive archive;

    if (argc < 2)
    {
        std::cout <<
            "Usage:\n"
            "  archive create <archive> <files...>\n"
            "  archive list <archive>\n"
            "  archive extract <archive> <dir>\n"
            "  archive info <archive>\n";

        return 1;
    }

    std::string command = argv[1];

    // ---------------- CREATE ----------------
    if (command == "create")
    {
        if (argc < 4)
        {
            std::cerr << "Not enough arguments\n";
            return 1;
        }

        std::string archiveName = argv[2];

        std::vector<std::string> files;

        for (int i = 3; i < argc; i++)
            files.push_back(argv[i]);

        if (!archive.create(archiveName, files))
            return 1;

        return 0;
    }

    // ---------------- LIST ----------------
    if (command == "list")
    {
        if (argc < 3)
        {
            std::cerr << "No archive specified\n";
            return 1;
        }

        return archive.list(argv[2]) ? 0 : 1;
    }

    // ---------------- EXTRACT ----------------
    if (command == "extract")
    {
        if (argc < 4)
        {
            std::cerr << "Usage: extract <archive> <dir>\n";
            return 1;
        }

        return archive.extract(argv[2], argv[3]) ? 0 : 1;
    }

    // ---------------- INFO ----------------
    if (command == "info")
    {
        if (argc < 3)
        {
            std::cerr << "No archive specified\n";
            return 1;
        }

        return archive.info(argv[2]) ? 0 : 1;
    }

    // ---------------- ADD ----------------
    if (command == "add")
    {
        if (argc < 4)
        {
            std::cerr << "Usage: add <archive> <files...>\n";
            return 1;
        }

        std::string archiveName = argv[2];

        std::vector<std::string> files;

        for (int i = 3; i < argc; i++)
            files.push_back(argv[i]);

        return archive.add(archiveName, files) ? 0 : 1;
    }

    // ---------------- REMOVE ----------------
    if (command == "remove")
    {
        if (argc < 4)
        {
            std::cerr << "Usage: remove <archive> <file>\n";
            return 1;
        }

        return archive.remove(argv[2], argv[3]) ? 0 : 1;
    }

    std::cerr << "Unknown command\n";
    return 1;
}