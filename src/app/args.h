#pragma once

#include <filesystem>

struct Args {
    std::filesystem::path dumpFile;
};

Args parseArgs(int argc, char* argv[]);
