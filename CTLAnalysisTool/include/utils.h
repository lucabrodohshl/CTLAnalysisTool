
#pragma once
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>
#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include "types.h"
#include "analysis_result.h"
//#include "Analyzers/Refinement.h"
namespace ctl
{
    // Helper function to check if a path exists
bool pathExists(const std::string& path);
// Helper function to check if path is a directory
bool isDirectory(const std::string& path) ;

// Helper function to create directory
bool createDirectory(const std::string& path);
// Helper function to get all .txt files in a directory
std::vector<std::string> getTextFilesInDirectory(const std::string& dir_path);

std::vector<std::string> loadPropertiesFromFile(const std::string& filename);
std::string joinPaths(const std::string& path1, const std::string& path2);
std::vector<std::string> getSubdirectoriesInDirectory(const std::string& dir_path);
} // namespace ctl


