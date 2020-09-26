#pragma once

#include <memory>

#include <common/vk_util.hpp>

#include <emulator/core/hw/ppu/vulkan_render/frontend.hpp>

struct SDL_Window;

namespace nba {
struct Config;
}

class SDL2_VK_Frontend final : public nba::core::VulkanFrontend {
public:
    SDL2_VK_Frontend(const nba::Config& config);
    ~SDL2_VK_Frontend();

    virtual vk::Instance GetVulkanInstance() const override;
    virtual vk::SurfaceKHR GetDisplaySurface() const override;
    virtual vk::Extent2D GetDisplayDimensions() const override;

    SDL_Window* GetWindow() {
        return window.get();
    }

private:

    const nba::Config& config;
    int window_width{}, window_height{};

    std::unique_ptr<SDL_Window, void (*)(SDL_Window*)> window{nullptr, nullptr};

    vk::UniqueInstance vk_instance;

    vk::UniqueSurfaceKHR vk_window_surface;
};
