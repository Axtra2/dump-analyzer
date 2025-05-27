#pragma once

#include <bit>
#include <filesystem>
#include <vector>

std::vector<std::byte> readWholeFile(const std::filesystem::path& path);
