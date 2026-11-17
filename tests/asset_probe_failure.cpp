#include <exception>
#include <filesystem>
#include <iostream>

// Native process-boundary fixture: also executable on Windows without a shell.
int main(int argc, char **argv) try {
    if (argc < 1)
        return 2;
    if (std::filesystem::path(argv[0]).stem() == "diagnostic-probe") {
        std::cout << "EXPECTED_SCENE_VERIFY_FAILURE\n";
        return 7;
    }
    std::cout << R"({"package_valid":0,"lod_triangles":[1,1,1],"texture_bytes":0})" << '\n';
    return 0;
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
}
