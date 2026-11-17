#include "assetc/metadata.hpp"
#include <algorithm>
#include <set>
#include <simdjson.h>
#include <string_view>

namespace ngen::assetc {
namespace {
std::optional<ImportFailure> shape(simdjson::dom::element value, std::size_t depth,
                                   std::size_t &remaining) {
    if (depth > 64 || remaining == 0)
        return ImportFailure{ImportIssue::capacity, "JSON metadata depth or value limit exceeded"};
    --remaining;
    if (value.is_object()) {
        simdjson::dom::object object;
        if (value.get_object().get(object))
            return ImportFailure{ImportIssue::invalid, "invalid JSON metadata object"};
        std::set<std::string_view> keys;
        for (auto field : object) {
            if (!keys.insert(field.key).second)
                return ImportFailure{ImportIssue::invalid,
                                     "duplicate JSON object keys are not admitted"};
            if (auto failure = shape(field.value, depth + 1, remaining))
                return failure;
        }
    } else if (value.is_array()) {
        simdjson::dom::array array;
        if (value.get_array().get(array))
            return ImportFailure{ImportIssue::invalid, "invalid JSON metadata array"};
        for (auto element : array)
            if (auto failure = shape(element, depth + 1, remaining))
                return failure;
    }
    return std::nullopt;
}
} // namespace
std::optional<ImportFailure> check_metadata(std::span<const std::byte> json) {
    simdjson::dom::parser parser;
    simdjson::dom::element document;
    const auto error =
        parser.parse(reinterpret_cast<const char *>(json.data()), json.size()).get(document);
    if (error)
        return ImportFailure{error == simdjson::MEMALLOC ? ImportIssue::capacity
                                                         : ImportIssue::invalid,
                             "GLB JSON metadata parsing failed"};
    std::size_t remaining = 262144;
    if (auto failure = shape(document, 0, remaining))
        return failure;
    simdjson::dom::object root;
    if (document.get_object().get(root))
        return ImportFailure{ImportIssue::invalid, "GLB JSON root must be an object"};
    constexpr std::array<std::pair<std::string_view, std::size_t>, 14> limits{
        {{"meshes", 4096},
         {"nodes", 4096},
         {"materials", 256},
         {"images", 64},
         {"textures", 256},
         {"samplers", 64},
         {"accessors", 32768},
         {"bufferViews", 32768},
         {"scenes", 16},
         {"buffers", 1},
         {"extensionsUsed", 16},
         {"extensionsRequired", 16},
         {"animations", 32},
         {"skins", 32}}};
    for (const auto &[name, limit] : limits) {
        auto field = root[name];
        if (field.error() == simdjson::NO_SUCH_FIELD)
            continue;
        simdjson::dom::array array;
        if (field.get_array().get(array))
            return ImportFailure{ImportIssue::invalid,
                                 std::string{name} + " metadata must be an array"};
        if (array.size() > limit)
            return ImportFailure{ImportIssue::capacity,
                                 std::string{name} + " metadata count limit exceeded"};
        if ((name == "animations" || name == "skins") && array.size() != 0)
            return ImportFailure{ImportIssue::unsupported,
                                 "animation and skinning are outside the static subset"};
        for (auto element : array) {
            if (name == "extensionsUsed" || name == "extensionsRequired") {
                std::string_view extension;
                if (element.get_string().get(extension) || extension.size() > 128 ||
                    !std::all_of(extension.begin(), extension.end(), [](unsigned char c) {
                        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                               (c >= '0' && c <= '9') || c == '_';
                    }))
                    return ImportFailure{ImportIssue::invalid, "invalid extension name"};
                if (extension != "KHR_lights_punctual" &&
                    extension != "KHR_materials_emissive_strength")
                    return ImportFailure{ImportIssue::unsupported,
                                         "unsupported extension: " + std::string{extension}};
            }
            if (name == "buffers" || name == "images") {
                simdjson::dom::object object;
                if (element.get_object().get(object))
                    return ImportFailure{ImportIssue::invalid,
                                         "buffer/image metadata must be an object"};
                if (object["uri"].error() != simdjson::NO_SUCH_FIELD)
                    return ImportFailure{
                        ImportIssue::unsupported,
                        "buffer/image URIs are not admitted; use embedded GLB data"};
                if (name == "buffers") {
                    std::uint64_t bytes = 0;
                    if (object["byteLength"].get_uint64().get(bytes))
                        return ImportFailure{ImportIssue::invalid,
                                             "buffer byteLength must be an unsigned integer"};
                    if (bytes > source_byte_limit)
                        return ImportFailure{ImportIssue::capacity,
                                             "declared buffer exceeds source byte limit"};
                }
            }
        }
    }
    return std::nullopt;
}
} // namespace ngen::assetc
