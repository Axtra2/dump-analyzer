#include <utils/fs_utils.h>

#include <fstream>

std::vector<std::byte> readWholeFile(const std::filesystem::path& path) {
    std::ifstream fin;
    fin.exceptions(std::ios_base::badbit | std::ios_base::failbit | std::ios_base::eofbit);
    fin.open(path, std::ios_base::binary | std::ios_base::in | std::ios_base::ate);
    std::vector<std::byte> bytes(fin.tellg());
    fin.seekg(std::ios_base::beg);
    fin.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    return bytes;
}
