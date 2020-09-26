#pragma once

#include <memory>

#include <platform/sdl/vk_util.hpp>

#include <emulator/device/video_device.hpp>

struct SDL_Window;

namespace nba {
struct Config;
}

struct SDL2_VK_VideoDevice : public nba::VideoDevice {
    static constexpr unsigned native_width = 240, native_height = 160;

    const nba::Config& config;
    int window_width{}, window_height{};

    std::unique_ptr<SDL_Window, void (*)(SDL_Window*)> window{nullptr, nullptr};

    vk::UniqueInstance vk_instance;
    vk::PhysicalDevice vk_physical_device;
    std::uint32_t queue_family_index;
    vk::Queue vk_queue;
    vk::UniqueDevice vk_device;

    vk::UniqueSurfaceKHR vk_window_surface;
    vk::Format vk_window_surface_format;
    vk::UniqueSwapchainKHR vk_swapchain;
    std::vector<vk::Image> vk_swapchain_images;
    //std::vector<vk::UniqueImageView> vk_swapchain_image_views;

    struct {
        vk::UniqueImage image;
        vk::UniqueDeviceMemory memory;
        //vk::UniqueImageView view;
        vk::ImageLayout layout = vk::ImageLayout::eUndefined;
        std::uint32_t* ptr = nullptr;
    } staging;

    vk::UniqueFence draw_fence;
    vk::UniqueCommandPool cmd_pool;
    std::vector<vk::UniqueCommandBuffer> blit_cmd_buffers;
    vk::UniqueSemaphore image_available_semaphore;
    vk::UniqueSemaphore blit_finished_semaphore;

    SDL2_VK_VideoDevice(const nba::Config& config);
    ~SDL2_VK_VideoDevice();

    void Draw(std::uint32_t* buffer) final;
};
