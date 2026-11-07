#include "assetc/source.hpp"
#include "assetc/import.hpp"
#include <fstream>
#include <stdexcept>
namespace ngen::assetc {
std::vector<std::byte> read_source(const std::filesystem::path &path) {
    if (!std::filesystem::is_regular_file(path))
        throw std::runtime_error("source is not a readable regular file");
    const auto size = std::filesystem::file_size(path);
    if (size > source_byte_limit)
        throw std::runtime_error("source byte limit exceeded");
    std::ifstream input(path, std::ios::binary);
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (!bytes.empty())
        input.read(reinterpret_cast<char *>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    if (!input || input.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("source read failed or file size changed");
    return bytes;
}
} // namespace ngen::assetc
