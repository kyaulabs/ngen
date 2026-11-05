#include "render/context.hpp"
#include <cstdio>
#include <string_view>

int main(int argc, char **argv) try {
    ngen::render::Context::Options options;
    options.visible = false;
    if (argc == 2 && std::string_view(argv[1]) == "--without-validation")
        options.validation = false;
    else if (argc != 1) {
        std::fputs("Usage: render_probe [--without-validation]\n", stderr);
        return 2;
    }
    auto result = ngen::render::Context::create(options);
    if (const auto *failure = std::get_if<ngen::render::Failure>(&result)) {
        std::fprintf(stderr, "Vulkan startup: %s\n", failure->message.c_str());
        return 1;
    }
    const auto &context = std::get<std::unique_ptr<ngen::render::Context>>(result);
    const bool closed = context->close();
    return closed && context->close() && context->device() == VK_NULL_HANDLE &&
                   context->physical_device() == VK_NULL_HANDLE &&
                   context->queue() == VK_NULL_HANDLE && context->surface() == VK_NULL_HANDLE &&
                   context->instance() == VK_NULL_HANDLE && context->window() == nullptr &&
                   context->queue_family() == UINT32_MAX
               ? 0
               : 1;
} catch (const std::exception &error) {
    std::fprintf(stderr, "Vulkan probe: %s\n", error.what());
    return 1;
}
