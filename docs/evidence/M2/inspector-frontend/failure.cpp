// Process-local native controls. No engine policy or validation is disabled.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vulkan/vulkan.h>
namespace {
std::atomic<bool> overlay{}, recorded{}, fired{};
bool mode(const char *name) {
    const auto *value = std::getenv("NGEN_OVERLAY_FAILURE");
    return value && std::strcmp(value, name) == 0;
}
VkResult fail() {
    fired = true;
    std::fputs("overlay_failure_injected=1\n", stderr);
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}
template <class T> T next(const char *name) { return reinterpret_cast<T>(dlsym(RTLD_NEXT, name)); }
} // namespace
extern "C" VKAPI_ATTR VkResult VKAPI_CALL
vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo *info,
                       const VkAllocationCallbacks *allocation, VkPipelineLayout *layout) {
    if (info->pushConstantRangeCount == 1 &&
        info->pPushConstantRanges[0].stageFlags == VK_SHADER_STAGE_VERTEX_BIT &&
        info->pPushConstantRanges[0].size == 16) {
        overlay = true;
        if (mode("layout"))
            return fail();
    }
    return next<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(device, info, allocation,
                                                                      layout);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL
vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo *info,
                       const VkAllocationCallbacks *allocation, VkDescriptorPool *pool) {
    if (overlay && mode("pool"))
        return fail();
    return next<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(device, info, allocation,
                                                                      pool);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkAllocateDescriptorSets(
    VkDevice device, const VkDescriptorSetAllocateInfo *info, VkDescriptorSet *sets) {
    if (overlay && mode("sets"))
        return fail();
    return next<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(device, info, sets);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateSampler(VkDevice device,
                                                          const VkSamplerCreateInfo *info,
                                                          const VkAllocationCallbacks *allocation,
                                                          VkSampler *sampler) {
    if (overlay && mode("sampler"))
        return fail();
    return next<PFN_vkCreateSampler>("vkCreateSampler")(device, info, allocation, sampler);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL
vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo *info,
                     const VkAllocationCallbacks *allocation, VkShaderModule *module) {
    static std::atomic<unsigned> calls{};
    if (overlay && mode("shader2") && ++calls == 2)
        return fail();
    return next<PFN_vkCreateShaderModule>("vkCreateShaderModule")(device, info, allocation, module);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL
vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache cache, std::uint32_t count,
                          const VkGraphicsPipelineCreateInfo *infos,
                          const VkAllocationCallbacks *allocation, VkPipeline *pipelines) {
    static std::atomic<unsigned> calls{};
    if (overlay && mode("pipeline2") && ++calls == 2)
        return fail();
    return next<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(
        device, cache, count, infos, allocation, pipelines);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateBuffer(VkDevice device,
                                                         const VkBufferCreateInfo *info,
                                                         const VkAllocationCallbacks *allocation,
                                                         VkBuffer *buffer) {
    if (overlay && mode("buffer"))
        return fail();
    return next<PFN_vkCreateBuffer>("vkCreateBuffer")(device, info, allocation, buffer);
}
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexed(VkCommandBuffer commands,
                                                       std::uint32_t indices,
                                                       std::uint32_t instances, std::uint32_t first,
                                                       std::int32_t vertex,
                                                       std::uint32_t instance) {
    if (overlay && mode("submit"))
        recorded = true;
    next<PFN_vkCmdDrawIndexed>("vkCmdDrawIndexed")(commands, indices, instances, first, vertex,
                                                   instance);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit2(VkQueue queue, std::uint32_t count,
                                                         const VkSubmitInfo2 *submits,
                                                         VkFence fence) {
    if (mode("submit") && recorded.exchange(false) && !fired)
        return fail();
    return next<PFN_vkQueueSubmit2>("vkQueueSubmit2")(queue, count, submits, fence);
}
