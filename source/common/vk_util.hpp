#pragma once

#include <atomic>
#include <cstdint>
#include <string_view>

#include <common/log.hpp>

using u32 = std::uint32_t;

namespace vk {
class DispatchLoaderDynamic;
}

namespace Vulkan {
inline vk::DispatchLoaderDynamic& GetDefaultDispatcher();
}

#define NOMINMAX
#define VULKAN_HPP_DEFAULT_DISPATCHER_TYPE ::vk::DispatchLoaderDynamic
#define VULKAN_HPP_DEFAULT_DISPATCHER ::Vulkan::GetDefaultDispatcher()
#include <vulkan/vulkan.hpp>
#undef interface

inline vk::DynamicLoader dl;

static VKAPI_ATTR vk::Bool32 VKAPI_CALL
VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
    auto level = static_cast<common::logger::Level>(__builtin_ffs(messageSeverity));
    common::logger::append(level, __FILE__, __func__, __LINE__, pCallbackData->pMessage);
    return VK_FALSE;
}

namespace Vulkan {

inline vk::DispatchLoaderDynamic& GetDefaultDispatcher() {
    static vk::DispatchLoaderDynamic loader;
    return loader;
}

inline void InitDispatcher(PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr) {
    static std::atomic_bool initialized{false};
    if (initialized)
        return;
    initialized = true;
    if (!vkGetInstanceProcAddr)
        vkGetInstanceProcAddr =
            dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
}

inline vk::UniqueInstance CreateInstance(std::string_view app_name, std::uint32_t version,
                                         vk::ArrayProxy<const char*> validation_layers,
                                         vk::ArrayProxy<const char*> instance_extensions) {
    vk::ApplicationInfo info;
    info.setApiVersion(version);
    info.setPApplicationName(app_name.data());
    vk::InstanceCreateInfo create_info;
    create_info.setPApplicationInfo(&info);
    create_info.setEnabledLayerCount(validation_layers.size());
    create_info.setPpEnabledLayerNames(validation_layers.data());
    create_info.setEnabledExtensionCount(instance_extensions.size());
    create_info.setPpEnabledExtensionNames(instance_extensions.data());

    vk::UniqueInstance instance = vk::createInstanceUnique(create_info);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
    return instance;
}

inline vk::UniqueDebugUtilsMessengerEXT CreateDebugMessenger(vk::Instance instance) {
    vk::DebugUtilsMessengerCreateInfoEXT messenger_info;
    messenger_info.setPfnUserCallback(VulkanDebugCallback);
    constexpr auto severity_flags{vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eError};
    constexpr auto type_flags{vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                              vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                              vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation};
    messenger_info.setMessageSeverity(severity_flags);
    messenger_info.setMessageType(type_flags);
    return instance.createDebugUtilsMessengerEXTUnique(messenger_info);
}

inline std::uint32_t GetQueueFamilyIndex(vk::PhysicalDevice physical_device,
                                         vk::QueueFlags queue_flags) {

    auto queue_families = physical_device.getQueueFamilyProperties();
    for (std::uint32_t queue_family_index{0}; queue_family_index != queue_families.size();
         ++queue_family_index) {
        const auto& queue_family = queue_families[queue_family_index];
        if (queue_family.queueFlags & queue_flags)
            return queue_family_index;
    }
    ASSERT(false, "Failed to find suitable queue family! Physical Device: {}, Properties: {}\n",
           physical_device.getProperties().deviceName, vk::to_string(queue_flags));
}

inline std::uint32_t GetMemoryType(vk::PhysicalDevice physical_device, std::uint32_t type_filter,
                                   vk::MemoryPropertyFlags properties) {
    const auto physical_memory_properties = physical_device.getMemoryProperties();
    for (std::uint32_t idx = 0; idx < physical_memory_properties.memoryTypeCount; idx++) {
        if ((type_filter & (1 << idx)) &&
            (physical_memory_properties.memoryTypes[idx].propertyFlags & properties) ==
                properties) {
            return idx;
        }
    }
    ASSERT(false, "Failed to find suitable memory type! Filter: {:#010X}, Properties: {}\n",
           type_filter, vk::to_string(properties));
}

} // namespace Vulkan
