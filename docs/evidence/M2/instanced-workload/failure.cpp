#include <vulkan/vulkan.h>
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <dlfcn.h>

namespace {
bool control(const char *name) {
    const auto *value = std::getenv("NGEN_WORKLOAD_CONTROL");
    return value && std::strcmp(value, name) == 0;
}
template <class T> T next(const char *name) { return reinterpret_cast<T>(dlsym(RTLD_NEXT, name)); }
}
extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice physical, std::uint32_t *count, VkQueueFamilyProperties *properties) {
    next<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties")(
        physical, count, properties);
    if (properties && (control("unsupported") || control("wrap")))
        for (std::uint32_t i = 0; i < *count; ++i)
            properties[i].timestampValidBits = control("unsupported") ? 0U : 36U;
}
extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties(
    VkPhysicalDevice physical, VkPhysicalDeviceProperties *properties) {
    next<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(physical, properties);
    if (control("wrap"))
        properties->limits.timestampPeriod = 2;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateQueryPool(
    VkDevice device, const VkQueryPoolCreateInfo *info, const VkAllocationCallbacks *allocator,
    VkQueryPool *pool) {
    if (control("create"))
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    return next<PFN_vkCreateQueryPool>("vkCreateQueryPool")(device, info, allocator, pool);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetQueryPoolResults(
    VkDevice device, VkQueryPool pool, std::uint32_t first, std::uint32_t count, std::size_t size,
    void *data, VkDeviceSize stride, VkQueryResultFlags flags) {
    const auto result = next<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults")(
        device, pool, first, count, size, data, stride, flags);
    if (control("wrap") && result == VK_SUCCESS && (flags & VK_QUERY_RESULT_64_BIT))
        for (std::uint32_t i = 0; i < count; ++i) {
            const std::uint64_t value = i % 2 == 0 ? (std::uint64_t{1} << 36) - 10 : 6;
            std::memcpy(static_cast<char *>(data) + i * stride, &value, sizeof(value));
        }
    return result;
}
extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexed(
    VkCommandBuffer commands, std::uint32_t indices, std::uint32_t instances,
    std::uint32_t first, std::int32_t offset, std::uint32_t first_instance) {
    if (control("instances") && instances > 1)
        instances = 1;
    next<PFN_vkCmdDrawIndexed>("vkCmdDrawIndexed")(
        commands, indices, instances, first, offset, first_instance);
}
