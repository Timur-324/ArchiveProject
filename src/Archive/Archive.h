#pragma once

#include "FileEntry.h"
#include <fstream>//библиотека для чтения/записи данных в файл
#include <string>
#include <vector>

class Archive
{
public:
    bool create(
        const std::string& archiveName,
        const std::vector<std::string>& files);

    bool add(
        const std::string& archiveName,
        const std::vector<std::string>& files);

    bool remove(
        const std::string& archiveName,
        const std::string& fileName);

    bool list(
        const std::string& archiveName);

    bool extract(
        const std::string& archiveName,
        const std::string& outputDirectory);

    bool info(
        const std::string& archiveName);  
};
