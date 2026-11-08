#include "render/shader_contract.hpp"

#include <initializer_list>
#include <iostream>
#include <string_view>
#include <utility>

namespace {
void report(std::string_view name, std::size_t size, std::size_t alignment,
            std::initializer_list<std::pair<std::string_view, std::size_t>> fields) {
    std::cout << name << ' ' << size << ' ' << alignment;
    for (auto [field, offset] : fields) {
        std::cout << ' ' << field << ':' << offset;
    }
    std::cout << '\n';
}
} // namespace

int main() {
    using namespace ngen::render::shader;
    report("FrameConstants", sizeof(FrameConstants), alignof(FrameConstants),
           {{"viewProjection", offsetof(FrameConstants, viewProjection)},
            {"model", offsetof(FrameConstants, model)},
            {"normalMatrix", offsetof(FrameConstants, normalMatrix)},
            {"lightViewProjection", offsetof(FrameConstants, lightViewProjection)},
            {"cameraPosition", offsetof(FrameConstants, cameraPosition)},
            {"keyDirectionIntensity", offsetof(FrameConstants, keyDirectionIntensity)},
            {"keyColor", offsetof(FrameConstants, keyColor)}});
    report("InstanceConstants", sizeof(InstanceConstants), alignof(InstanceConstants),
           {{"transforms", offsetof(InstanceConstants, transforms)}});
    report("InstanceTransform", sizeof(InstanceTransform), alignof(InstanceTransform),
           {{"model", offsetof(InstanceTransform, model)},
            {"normalMatrix", offsetof(InstanceTransform, normalMatrix)}});
    report(
        "LightConstants", sizeof(LightConstants), alignof(LightConstants),
        {{"lights", offsetof(LightConstants, lights)}, {"count", offsetof(LightConstants, count)}});
    report("LocalLight", sizeof(LocalLight), alignof(LocalLight),
           {{"positionRange", offsetof(LocalLight, positionRange)},
            {"colorIntensity", offsetof(LocalLight, colorIntensity)}});
    report("MaterialConstants", sizeof(MaterialConstants), alignof(MaterialConstants),
           {{"baseColor", offsetof(MaterialConstants, baseColor)},
            {"emissiveAlphaCutoff", offsetof(MaterialConstants, emissiveAlphaCutoff)},
            {"metallicRoughnessNormalOcclusion",
             offsetof(MaterialConstants, metallicRoughnessNormalOcclusion)},
            {"flags", offsetof(MaterialConstants, flags)}});
    report(
        "CullConstants", sizeof(CullConstants), alignof(CullConstants),
        {{"planes", offsetof(CullConstants, planes)}, {"count", offsetof(CullConstants, count)}});
    report("PostConstants", sizeof(PostConstants), alignof(PostConstants),
           {{"exposureFxaaTexel", offsetof(PostConstants, exposureFxaaTexel)},
            {"bloom", offsetof(PostConstants, bloom)}});
    report("OverlayConstants", sizeof(OverlayConstants), alignof(OverlayConstants),
           {{"scale_translate", offsetof(OverlayConstants, scale_translate)}});
    report("OverlayVertex", sizeof(OverlayVertex), alignof(OverlayVertex),
           {{"position", offsetof(OverlayVertex, position)},
            {"uv", offsetof(OverlayVertex, uv)},
            {"color", offsetof(OverlayVertex, color)}});
    report("Vertex", sizeof(Vertex), alignof(Vertex),
           {{"position", offsetof(Vertex, position)},
            {"normal", offsetof(Vertex, normal)},
            {"uv", offsetof(Vertex, uv)},
            {"uv1", offsetof(Vertex, uv1)},
            {"tangent", offsetof(Vertex, tangent)}});
}
