#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <emulator/config/config.hpp>

#include "vk_renderer_frontend.hpp"

SDL2_VK_Frontend::SDL2_VK_Frontend(const nba::Config& config) : config{config} {
    window = {SDL_CreateWindow("Vulkan", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               native_width * config.video.scale,
                               native_height * config.video.scale, SDL_WINDOW_VULKAN),
              SDL_DestroyWindow};
    Vulkan::InitDispatcher(
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr()));
    {
        unsigned int sdl_instance_extension_count{0};
        SDL_Vulkan_GetInstanceExtensions(window.get(), &sdl_instance_extension_count, nullptr);
        std::vector<const char*> instance_extensions(sdl_instance_extension_count);
        SDL_Vulkan_GetInstanceExtensions(window.get(), &sdl_instance_extension_count,
                                         instance_extensions.data());
        instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        std::array layers{"VK_LAYER_KHRONOS_validation"};
        vk_instance =
            Vulkan::CreateInstance("NanoboyAdvance", VK_VERSION_1_2, layers, instance_extensions);
    }
    {
        VkSurfaceKHR surface;
        ASSERT(SDL_Vulkan_CreateSurface(window.get(), *vk_instance, &surface),
               "Failed to create window surface!");
        vk_window_surface.reset(surface);
    }
}

vk::Instance SDL2_VK_Frontend::GetVulkanInstance() const {
    return *vk_instance;
}

vk::SurfaceKHR SDL2_VK_Frontend::GetDisplaySurface() const {
    return *vk_window_surface;
}

vk::Extent2D SDL2_VK_Frontend::GetDisplayDimensions() const {
    int w, h;
    // Recommended way to get window image size for SDL2
    // https://wiki.libsdl.org/SDL_Vulkan_GetDrawableSize
    SDL_Vulkan_GetDrawableSize(window.get(), &w, &h);
    return {static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h)};
}

SDL2_VK_Frontend::~SDL2_VK_Frontend() = default;
