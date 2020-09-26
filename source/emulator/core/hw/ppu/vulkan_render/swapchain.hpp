#pragma once

#include "frontend.hpp"

namespace nba::core {

struct Swapchain {
public:
    Swapchain() = default;
    // cmd_buffer should be already active
    Swapchain(vk::PhysicalDevice physical_device, vk::Device device, std::uint32_t queue_family_idx,
              vk::Queue queue, vk::CommandBuffer cmd_buffer, const VulkanFrontend& frontend);

    const std::vector<vk::Image>& GetImages() const {
        return images;
    }

    const std::vector<vk::UniqueImageView>& GetImageViews() const {
        return image_views;
    }

    vk::SurfaceFormatKHR GetSurfaceFormat() const {
        return surface_format;
    }

    struct IndexSemaphore {
        std::uint32_t image_index;
        vk::Semaphore acquire_semaphore;
    };
    IndexSemaphore AcquireImage();
    void SwapImages(std::uint32_t index, vk::Semaphore swap_semaphore);

private:
    std::vector<vk::Image> images;
    std::vector<vk::UniqueImageView> image_views;
    vk::UniqueSwapchainKHR swapchain;
    vk::UniqueSemaphore image_available_semaphore;
    vk::Queue queue;
    vk::SurfaceFormatKHR surface_format;
};

} // namespace nba::core
