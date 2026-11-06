#include "render/context.hpp"
#include "render/capabilities.hpp"
#include "render/presentation.hpp"
#include "render/resources_internal.hpp"
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
SurfaceFailure surface_query_failure(VkResult result, const char *operation) {
    return {SurfaceIssue::query_failed, result,
            std::string(operation) + ": VkResult " + std::to_string(result)};
}
SurfaceSelection query_surface(VkPhysicalDevice physical, VkSurfaceKHR surface,
                               VkExtent2D drawable) {
    VkSurfaceCapabilitiesKHR capabilities;
    auto result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &capabilities);
    if (result != VK_SUCCESS)
        return surface_query_failure(result, "query surface capabilities");
    std::uint32_t count = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, nullptr);
    if (result != VK_SUCCESS)
        return surface_query_failure(result, "count surface formats");
    if (count > 256)
        return SurfaceFailure{SurfaceIssue::invalid_capabilities, VK_SUCCESS,
                              "too many surface formats"};
    std::vector<VkSurfaceFormatKHR> formats(count);
    if (count != 0) {
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, formats.data());
        if (result != VK_SUCCESS)
            return surface_query_failure(result, "query surface formats");
        formats.resize(count);
    }
    count = 0;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, nullptr);
    if (result != VK_SUCCESS)
        return surface_query_failure(result, "count presentation modes");
    if (count > 64)
        return SurfaceFailure{SurfaceIssue::invalid_capabilities, VK_SUCCESS,
                              "too many presentation modes"};
    std::vector<VkPresentModeKHR> modes(count);
    if (count != 0) {
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, modes.data());
        if (result != VK_SUCCESS)
            return surface_query_failure(result, "query presentation modes");
        modes.resize(count);
    }
    auto selection = select_surface(capabilities, formats, modes, drawable);
    if (const auto *plan = std::get_if<SurfacePlan>(&selection)) {
        VkFormatProperties format{};
        vkGetPhysicalDeviceFormatProperties(physical, plan->format.format, &format);
        if ((format.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
            return SurfaceFailure{SurfaceIssue::unsupported_format, VK_SUCCESS,
                                  "surface format cannot be a color attachment"};
    }
    return selection;
}
} // namespace
Context::Context() = default;
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
        if (options.presentation) {
            vkcheck(vkEnumerateInstanceExtensionProperties(layer, &count, nullptr),
                    "count validation extensions");
            require(count <= 64, "too many validation-layer extensions");
            std::vector<VkExtensionProperties> available(count);
            vkcheck(vkEnumerateInstanceExtensionProperties(layer, &count, available.data()),
                    "query validation extensions");
            available.resize(count);
            require(std::any_of(available.begin(), available.end(),
                                [](const auto &entry) {
                                    return std::strcmp(entry.extensionName,
                                                       VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME) ==
                                           0;
                                }),
                    "synchronization validation requires VK_EXT_validation_features");
            extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
        }
    }
    if (options.presentation) {
        std::uint32_t available_count = 0;
        vkcheck(vkEnumerateInstanceExtensionProperties(nullptr, &available_count, nullptr),
                "count instance extensions");
        require(available_count <= 512, "too many instance extensions");
        std::vector<VkExtensionProperties> available(available_count);
        vkcheck(vkEnumerateInstanceExtensionProperties(nullptr, &available_count, available.data()),
                "query instance extensions");
        available.resize(available_count);
        for (const auto *name : {VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
                                 VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME}) {
            require(std::any_of(available.begin(), available.end(),
                                [name](const auto &entry) {
                                    return std::strcmp(entry.extensionName, name) == 0;
                                }),
                    "presentation requires surface-maintenance and capabilities2 extensions");
            extensions.push_back(name);
        }
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
    const VkValidationFeatureEnableEXT synchronize =
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    VkValidationFeaturesEXT validation{};
    validation.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validation.pNext = &debug;
    validation.enabledValidationFeatureCount = 1;
    validation.pEnabledValidationFeatures = &synchronize;
    VkInstanceCreateInfo instance{};
    instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance.pApplicationInfo = &application;
    instance.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    instance.ppEnabledExtensionNames = extensions.data();
    if (options.validation) {
        instance.enabledLayerCount = 1;
        instance.ppEnabledLayerNames = &layer;
        instance.pNext = options.presentation ? static_cast<const void *>(&validation)
                                              : static_cast<const void *>(&debug);
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
    int drawable_width = 0, drawable_height = 0;
    sdlcheck(SDL_GetWindowSizeInPixels(window_, &drawable_width, &drawable_height));
    require(drawable_width >= 0 && drawable_height >= 0, "negative drawable size");
    const VkExtent2D drawable{static_cast<std::uint32_t>(drawable_width),
                              static_cast<std::uint32_t>(drawable_height)};
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
        VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT maintenance{};
        maintenance.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
        if (options.presentation)
            features13.pNext = &maintenance;
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
            if (options.presentation &&
                (!maintenance.swapchainMaintenance1 ||
                 std::none_of(supported.begin(), supported.end(), [](const auto &entry) {
                     return std::strcmp(entry.extensionName,
                                        VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME) == 0;
                 }))) {
                reason += "presentation requires VK_EXT_swapchain_maintenance1; ";
                continue;
            }
            const auto selection = query_surface(candidate, surface_, drawable);
            if (const auto *failure = std::get_if<SurfaceFailure>(&selection);
                failure && failure->issue != SurfaceIssue::not_drawable) {
                reason += "queue " + std::to_string(family) + ": " + failure->message + "; ";
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
    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT maintenance{};
    maintenance.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
    maintenance.swapchainMaintenance1 = VK_TRUE;
    if (options.presentation)
        features13.pNext = &maintenance;
    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = &features13;
    features12.timelineSemaphore = VK_TRUE;
    const std::array device_extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                       VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME};
    VkDeviceCreateInfo device{};
    device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device.pNext = &features12;
    device.queueCreateInfoCount = 1;
    device.pQueueCreateInfos = &queue;
    device.enabledExtensionCount = options.presentation ? 2U : 1U;
    device.ppEnabledExtensionNames = device_extensions.data();
    vkcheck(vkCreateDevice(physical_, &device, nullptr, &device_), "create device");
    vkGetDeviceQueue(device_, queue_family_, 0, &queue_);
    presentation_enabled_ = options.presentation;
}
SurfaceSelection Context::inspect_surface() const {
    if (!physical_ || !surface_ || !window_)
        return SurfaceFailure{SurfaceIssue::closed, VK_SUCCESS, "context is closed"};
    if ((SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) != 0)
        return SurfaceFailure{SurfaceIssue::not_drawable, VK_SUCCESS, "window is minimized"};
    int width = 0, height = 0;
    if (!SDL_GetWindowSizeInPixels(window_, &width, &height))
        return SurfaceFailure{SurfaceIssue::query_failed, VK_SUCCESS, SDL_GetError()};
    if (width < 0 || height < 0)
        return SurfaceFailure{SurfaceIssue::invalid_capabilities, VK_SUCCESS,
                              "negative drawable size"};
    return query_surface(physical_, surface_,
                         {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)});
}
std::variant<FrameOutcome, Failure> Context::render_frame() {
    if (!device_ || !presentation_enabled_ || presentation_failed_)
        return Failure{"presentation is unavailable; create a presentation-enabled context"};
    try {
        if (!presentation_) {
            auto presentation = std::make_unique<Presentation>(*this);
            presentation->initialize();
            presentation_ = std::move(presentation);
        }
        return presentation_->draw();
    } catch (const std::exception &error) {
        presentation_failed_ = true;
        return Failure{error.what()};
    }
}
std::variant<Buffer, Failure> Context::create_buffer(BufferDescription description) {
    if (!device_)
        return Failure{"resource context is closed"};
    try {
        if (!resources_)
            resources_ = std::make_shared<Resources>(*this);
        return resources_->create(description);
    } catch (const std::exception &error) {
        return Failure{error.what()};
    }
}
std::variant<Image, Failure> Context::create_image(ImageDescription description) {
    if (!device_)
        return Failure{"resource context is closed"};
    try {
        if (!resources_)
            resources_ = std::make_shared<Resources>(*this);
        return resources_->create(description);
    } catch (const std::exception &error) {
        return Failure{error.what()};
    }
}
std::variant<Submission, Failure> Context::copy_buffer(BufferCopy description) {
    if (!device_ || !resources_)
        return Failure{"resource context is unavailable"};
    try {
        return resources_->copy(description);
    } catch (const std::exception &error) {
        return Failure{error.what()};
    }
}
std::variant<Submission, Failure> Context::copy_to_image(ImageUpload description) {
    if (!device_ || !resources_)
        return Failure{"resource context is unavailable"};
    try {
        return resources_->copy(description);
    } catch (const std::exception &error) {
        return Failure{error.what()};
    }
}
std::variant<Submission, Failure> Context::copy_from_image(ImageReadback description) {
    if (!device_ || !resources_)
        return Failure{"resource context is unavailable"};
    try {
        return resources_->copy(description);
    } catch (const std::exception &error) {
        return Failure{error.what()};
    }
}
std::variant<Submission, Failure> Context::post_process(PostProcess description) {
    if (!device_ || !resources_)
        return Failure{"resource context is unavailable"};
    try {
        return resources_->post(description);
    } catch (const std::exception &error) {
        return Failure{error.what()};
    }
}
std::variant<bool, Failure> Context::submission_complete(const Submission &submission) {
    if (!device_ || !resources_)
        return Failure{"resource context is unavailable"};
    try {
        return resources_->completed(submission);
    } catch (const std::exception &error) {
        return Failure{error.what()};
    }
}
std::variant<ResourceStatistics, Failure> Context::resource_statistics() {
    if (!device_)
        return Failure{"resource context is closed"};
    try {
        return resources_ ? resources_->statistics() : ResourceStatistics{};
    } catch (const std::exception &error) {
        return Failure{error.what()};
    }
}
PresentationState Context::presentation_state() const {
    return presentation_ ? presentation_->state() : PresentationState{};
}
Context::~Context() {
    (void)close();
    // An unrecoverable driver failure must not leave a callback borrowing dead C++ state.
    destroy_messenger();
}
void Context::destroy_messenger() noexcept {
    if (messenger_) {
        const auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy)
            destroy(instance_, messenger_, nullptr);
    }
    messenger_ = VK_NULL_HANDLE;
}
bool Context::close() noexcept {
    if (device_) {
        // Startup/shutdown only; ordinary frame retirement must not idle the device.
        const auto result = vkDeviceWaitIdle(device_);
        if (result != VK_SUCCESS && close_result_ == VK_SUCCESS) {
            close_result_ = result;
            std::fprintf(stderr, "Vulkan shutdown: vkDeviceWaitIdle failed with VkResult %d\n",
                         result);
        }
        if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST)
            return false; // Completion is unproven; keep every handle alive for a retry.
        if (presentation_) {
            const auto retirement = presentation_->close(result);
            if (retirement != VK_SUCCESS) {
                if (close_result_ == VK_SUCCESS)
                    close_result_ = retirement;
                return false; // Keep ownership intact for a retry; never destroy in-use objects.
            }
            presentation_.reset();
        }
        if (resources_) {
            resources_->close();
            resources_.reset();
        }
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
        presentation_enabled_ = false;
    }
    queue_ = VK_NULL_HANDLE;
    physical_ = VK_NULL_HANDLE;
    queue_family_ = UINT32_MAX;
    if (surface_)
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
    destroy_messenger();
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
