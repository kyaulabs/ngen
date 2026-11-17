// Process-local Vulkan error controls for the Context-owned post pipeline.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

namespace {
bool mode(const char *name) {
    const auto *value = std::getenv("NGEN_POST_FAILURE");
    return value && std::strcmp(value, name) == 0;
}
VkResult injected() {
    std::fputs("post_control_injected=1\n", stderr);
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}
std::atomic<bool> draw_recorded{};
} // namespace
extern "C" VKAPI_ATTR VkResult VKAPI_CALL
vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo *info,
                       const VkAllocationCallbacks *allocation, VkDescriptorPool *pool) {
    if (mode("pool"))
        return injected();
    return reinterpret_cast<PFN_vkCreateDescriptorPool>(dlsym(RTLD_NEXT, "vkCreateDescriptorPool"))(
        device, info, allocation, pool);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkAllocateDescriptorSets(
    VkDevice device, const VkDescriptorSetAllocateInfo *info, VkDescriptorSet *sets) {
    if (mode("sets"))
        return injected();
    return reinterpret_cast<PFN_vkAllocateDescriptorSets>(
        dlsym(RTLD_NEXT, "vkAllocateDescriptorSets"))(device, info, sets);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateSampler(VkDevice device,
                                                          const VkSamplerCreateInfo *info,
                                                          const VkAllocationCallbacks *allocation,
                                                          VkSampler *sampler) {
    if (mode("sampler"))
        return injected();
    return reinterpret_cast<PFN_vkCreateSampler>(dlsym(RTLD_NEXT, "vkCreateSampler"))(
        device, info, allocation, sampler);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL
vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo *info,
                     const VkAllocationCallbacks *allocation, VkShaderModule *module) {
    static std::atomic<unsigned> calls{};
    if (mode("shader2") && ++calls == 2)
        return injected();
    return reinterpret_cast<PFN_vkCreateShaderModule>(dlsym(RTLD_NEXT, "vkCreateShaderModule"))(
        device, info, allocation, module);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL
vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache cache, std::uint32_t count,
                          const VkGraphicsPipelineCreateInfo *infos,
                          const VkAllocationCallbacks *allocation, VkPipeline *pipelines) {
    static std::atomic<unsigned> calls{};
    if (mode("pipeline2") && ++calls == 2)
        return injected();
    return reinterpret_cast<PFN_vkCreateGraphicsPipelines>(
        dlsym(RTLD_NEXT, "vkCreateGraphicsPipelines"))(device, cache, count, infos, allocation,
                                                       pipelines);
}
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdDraw(VkCommandBuffer commands, std::uint32_t vertices,
                                                std::uint32_t instances, std::uint32_t first_vertex,
                                                std::uint32_t first_instance) {
    if (mode("submit"))
        draw_recorded = true;
    reinterpret_cast<PFN_vkCmdDraw>(dlsym(RTLD_NEXT, "vkCmdDraw"))(commands, vertices, instances,
                                                                   first_vertex, first_instance);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit2(VkQueue queue, std::uint32_t count,
                                                         const VkSubmitInfo2 *submits,
                                                         VkFence fence) {
    if (mode("submit") && draw_recorded.exchange(false))
        return injected();
    return reinterpret_cast<PFN_vkQueueSubmit2>(dlsym(RTLD_NEXT, "vkQueueSubmit2"))(queue, count,
                                                                                    submits, fence);
}
