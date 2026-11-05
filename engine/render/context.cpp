#include "render/context.hpp"
#include "render/capabilities.hpp"
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace ngen::render {
namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void sdlcheck(bool result) {
    if (!result)
        throw std::runtime_error(SDL_GetError());
}
void vkcheck(VkResult result, const char *operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
}
} // namespace
std::variant<std::unique_ptr<Context>, Failure> Context::create(Options options) {
    try {
        auto context = std::unique_ptr<Context>(new Context);
        context->initialize(options);
        return context;
    } catch (const std::exception &error) {
        return Failure{error.what()};
    }
}
VKAPI_ATTR VkBool32 VKAPI_CALL Context::message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                VkDebugUtilsMessageTypeFlagsEXT,
                                                const VkDebugUtilsMessengerCallbackDataEXT *data,
                                                void *user) {
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        ++static_cast<Context *>(user)->validation_errors_;
    std::fprintf(stderr, "Vulkan validation: %s\n", data->pMessage);
    return VK_FALSE;
}
void Context::initialize(Options options) {
    sdlcheck(SDL_Init(SDL_INIT_VIDEO));
    video_ = true;
    SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
    if (!options.visible)
        flags |= SDL_WINDOW_HIDDEN;
    window_ = SDL_CreateWindow("NGEN Vulkan", 1280, 720, flags);
    require(window_ != nullptr, SDL_GetError());
    std::uint32_t count = 0;
    const auto *required = SDL_Vulkan_GetInstanceExtensions(&count);
    require(required != nullptr && count < 64, "SDL Vulkan instance extensions unavailable");
    std::vector<const char *> extensions(required, required + count);
    const char *layer = "VK_LAYER_KHRONOS_validation";
    if (options.validation) {
        vkcheck(vkEnumerateInstanceLayerProperties(&count, nullptr), "enumerate layers");
        require(count < 256, "too many Vulkan layers");
        std::vector<VkLayerProperties> layers(count);
        vkcheck(vkEnumerateInstanceLayerProperties(&count, layers.data()), "enumerate layers");
        require(
            std::any_of(layers.begin(), layers.end(),
                        [layer](const auto &p) { return std::strcmp(p.layerName, layer) == 0; }),
            "VK_LAYER_KHRONOS_validation required; install validation layers or use probe-only "
            "mode");
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    VkApplicationInfo application{};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "NGEN";
    application.apiVersion = VK_API_VERSION_1_3;
    VkDebugUtilsMessengerCreateInfoEXT debug{};
    debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug.messageType =
        VkDebugUtilsMessageTypeFlagsEXT{VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT} |
        VkDebugUtilsMessageTypeFlagsEXT{VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT} |
        VkDebugUtilsMessageTypeFlagsEXT{VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT};
    debug.pfnUserCallback = message;
    debug.pUserData = this;
    VkInstanceCreateInfo instance{};
    instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance.pApplicationInfo = &application;
    instance.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    instance.ppEnabledExtensionNames = extensions.data();
    if (options.validation) {
        instance.enabledLayerCount = 1;
        instance.ppEnabledLayerNames = &layer;
        instance.pNext = &debug;
    }
    vkcheck(vkCreateInstance(&instance, nullptr, &instance_), "create instance");
    if (options.validation) {
        const auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
        require(create != nullptr, "debug-utils entry point unavailable");
        vkcheck(create(instance_, &debug, nullptr, &messenger_), "create validation messenger");
    }
    sdlcheck(SDL_Vulkan_CreateSurface(window_, instance_, nullptr, &surface_));
    vkcheck(vkEnumeratePhysicalDevices(instance_, &count, nullptr), "enumerate devices");
    require(count > 0 && count <= 16, "expected 1..16 Vulkan physical devices");
    std::vector<VkPhysicalDevice> devices(count);
    vkcheck(vkEnumeratePhysicalDevices(instance_, &count, devices.data()), "enumerate devices");
    std::string rejections;
    constexpr std::array missing_names{
        "Vulkan 1.3",       "graphics queue",    "presentation queue",  "dynamicRendering",
        "synchronization2", "timelineSemaphore", "B8 color attachment", "D32 depth attachment"};
    for (auto candidate : devices) {
        std::string reason = "no queue families";
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(candidate, &properties);
        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        VkPhysicalDeviceVulkan12Features features12{};
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features12.pNext = &features13;
        VkPhysicalDeviceFeatures2 features{};
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext = &features12;
        vkGetPhysicalDeviceFeatures2(candidate, &features);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, nullptr);
        require(count <= 64, "too many queue families");
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, families.data());
        if (count != 0)
            reason.clear();
        for (std::uint32_t family = 0; family < count; ++family) {
            VkBool32 presentation = VK_FALSE;
            vkcheck(
                vkGetPhysicalDeviceSurfaceSupportKHR(candidate, family, surface_, &presentation),
                "query presentation support");
            VkFormatProperties color{}, depth{};
            vkGetPhysicalDeviceFormatProperties(candidate, VK_FORMAT_B8G8R8A8_SRGB, &color);
            vkGetPhysicalDeviceFormatProperties(candidate, VK_FORMAT_D32_SFLOAT, &depth);
            Capabilities capabilities{
                VK_API_VERSION_MAJOR(properties.apiVersion),
                VK_API_VERSION_MINOR(properties.apiVersion),
                (families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0,
                presentation != 0,
                features13.dynamicRendering != 0,
                features13.synchronization2 != 0,
                features12.timelineSemaphore != 0,
                (color.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0,
                (depth.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) !=
                    0};
            if (const auto missing = check(capabilities)) {
                reason += "queue " + std::to_string(family) + " lacks " +
                          missing_names[static_cast<std::size_t>(*missing)] + "; ";
                continue;
            }
            std::uint32_t extension_count = 0;
            vkcheck(
                vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count, nullptr),
                "query device extensions");
            require(extension_count <= 1024, "too many device extensions");
            std::vector<VkExtensionProperties> supported(extension_count);
            vkcheck(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count,
                                                         supported.data()),
                    "query device extensions");
            if (std::none_of(supported.begin(), supported.end(), [](const auto &e) {
                    return std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
                })) {
                reason +=
                    "queue " + std::to_string(family) + " requires missing VK_KHR_swapchain; ";
                continue;
            }
            physical_ = candidate;
            queue_family_ = family;
            std::printf("device=%s api=%u.%u driver=%u queue=%u display=%s\n",
                        properties.deviceName, capabilities.api_major, capabilities.api_minor,
                        properties.driverVersion, family, SDL_GetCurrentVideoDriver());
            break;
        }
        if (physical_)
            break;
        rejections += std::string(properties.deviceName) + ": " + reason + "\n";
    }
    if (!physical_)
        throw std::runtime_error("no compatible Vulkan device:\n" + rejections);
    const float priority = 1.0F;
    VkDeviceQueueCreateInfo queue{};
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.queueFamilyIndex = queue_family_;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;
    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = &features13;
    features12.timelineSemaphore = VK_TRUE;
    const char *swapchain = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo device{};
    device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device.pNext = &features12;
    device.queueCreateInfoCount = 1;
    device.pQueueCreateInfos = &queue;
    device.enabledExtensionCount = 1;
    device.ppEnabledExtensionNames = &swapchain;
    vkcheck(vkCreateDevice(physical_, &device, nullptr, &device_), "create device");
    vkGetDeviceQueue(device_, queue_family_, 0, &queue_);
}
Context::~Context() { (void)close(); }
bool Context::close() noexcept {
    if (device_) {
        // Startup/shutdown only; ordinary frame retirement must not idle the device.
        const auto result = vkDeviceWaitIdle(device_);
        if (result != VK_SUCCESS && close_result_ == VK_SUCCESS) {
            close_result_ = result;
            std::fprintf(stderr, "Vulkan shutdown: vkDeviceWaitIdle failed with VkResult %d\n",
                         result);
        }
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    queue_ = VK_NULL_HANDLE;
    physical_ = VK_NULL_HANDLE;
    queue_family_ = UINT32_MAX;
    if (surface_)
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
    if (messenger_) {
        const auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy)
            destroy(instance_, messenger_, nullptr);
    }
    messenger_ = VK_NULL_HANDLE;
    if (instance_)
        vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;
    if (window_)
        SDL_DestroyWindow(window_);
    window_ = nullptr;
    if (video_)
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    video_ = false;
    return close_result_ == VK_SUCCESS && validation_errors_.load() == 0;
}
} // namespace ngen::render
