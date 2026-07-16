// smallgine_pack: bundle an asset folder into one .sgpk package the engine can
// mount transparently (platform/vfs.hpp). Usage:
//   smallgine_pack <srcDir> <outFile> [keyPrefix]
// keyPrefix defaults to the source folder's own name, so
//   smallgine_pack assets assets.sgpk
// stores keys like "assets/test.tga" — matching the engine's asset keys. Drop
// the resulting file next to the executable and it is picked up automatically.
#include "tools/packager.hpp"
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    using namespace smallgine;
    if (argc < 3)
    {
        std::cerr << "usage: " << argv[0] << " <srcDir> <outFile> [keyPrefix]\n"
                  << "  e.g. " << argv[0] << " assets assets.sgpk\n";
        return 2;
    }

    std::string src = argv[1], out = argv[2];
    std::string prefix;
    if (argc >= 4) prefix = argv[3];
    else
    {
        // Folder name, robust to a trailing slash ("assets/" -> "assets").
        std::filesystem::path p(src);
        if (p.filename().empty()) p = p.parent_path();
        prefix = p.filename().string();
    }

    int n = packDirectory(src, out, prefix);
    if (n < 0) return 1;
    std::cout << "Packed " << n << " files from '" << src << "' into '" << out
              << "' (key prefix '" << prefix << "')\n";
    return 0;
}
