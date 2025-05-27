#include <app/args.h>

#include <argh.h>

#include <stdexcept>

Args parseArgs(int argc, char* argv[]) {
    (void)argc;
    Args         args;
    argh::parser cmdl(argv, argh::parser::PREFER_PARAM_FOR_UNREG_OPTION);
    if (!(cmdl("dump-file") >> args.dumpFile)) {
        throw std::runtime_error("--dump-file is required");
    }
    return args;
}
