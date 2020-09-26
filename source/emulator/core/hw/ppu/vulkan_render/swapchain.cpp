#include "swapchain.hpp"

namespace nba::core {
Swapchain::Swapchain(vk::PhysicalDevice physical_device, vk::Device device,
                     std::uint32_t queue_family_idx, vk::Queue queue, vk::CommandBuffer cmd_buffer,
                     const VulkanFrontend& frontend) : queue{queue} {
    vk::SurfaceKHR display_surface{frontend.GetDisplaySurface()};
    ASSERT(physical_device.getSurfaceSupportKHR(queue_family_idx, display_surface),
           "Window surface is not supported on this queue family!");
    {
        vk::SwapchainCreateInfoKHR swapchain_create_info;
        swapchain_create_info.surface = display_surface;
        auto surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(display_surface);

        surface_format = physical_device.getSurfaceFormatsKHR(display_surface)[0];
        swapchain_create_info.imageFormat = surface_format.format;
        swapchain_create_info.imageColorSpace = surface_format.colorSpace;

        // Prefer Mailbox mode for lower latency, otherwise default to FIFO
        {
            auto present_modes = physical_device.getSurfacePresentModesKHR(display_surface);
            swapchain_create_info.presentMode =
                std::find(present_modes.begin(), present_modes.end(),
                          vk::PresentModeKHR::eMailbox) == present_modes.end()
                    ? vk::PresentModeKHR::eFifo
                    : vk::PresentModeKHR::eMailbox;
        }

        // Choose swapchain image count
        swapchain_create_info.minImageCount = surface_capabilities.minImageCount + 1;
        if (surface_capabilities.maxImageCount != 0 &&
            swapchain_create_info.minImageCount > surface_capabilities.maxImageCount) {
            swapchain_create_info.minImageCount = surface_capabilities.maxImageCount;
        }

        swapchain_create_info.imageExtent = frontend.GetDisplayDimensions();
        swapchain_create_info.imageArrayLayers = 1;
        swapchain_create_info.imageUsage =
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment;
        swapchain_create_info.imageSharingMode = vk::SharingMode::eExclusive;
        swapchain_create_info.preTransform = surface_capabilities.currentTransform;
        swapchain_create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapchain_create_info.clipped = VK_TRUE;

        swapchain = device.createSwapchainKHRUnique(swapchain_create_info);
    }
    images = device.getSwapchainImagesKHR(*swapchain);
    // Create image views for all of the swapchain images
    {
        vk::ImageViewCreateInfo swapchain_image_view_create_info;
        swapchain_image_view_create_info.viewType = vk::ImageViewType::e2D;
        swapchain_image_view_create_info.format = surface_format.format;
        swapchain_image_view_create_info.subresourceRange.aspectMask =
            vk::ImageAspectFlagBits::eColor;
        swapchain_image_view_create_info.subresourceRange.levelCount = 1;
        swapchain_image_view_create_info.subresourceRange.layerCount = 1;

        for (auto image : images) {
            swapchain_image_view_create_info.image = image;
            image_views.emplace_back(
                device.createImageViewUnique(swapchain_image_view_create_info));
        }
    }
    // Create Semaphore for image Acquisition
    image_available_semaphore = device.createSemaphoreUnique({});
    // Perform initial layout transition
    std::vector<vk::ImageMemoryBarrier> barriers;
    {
        vk::ImageMemoryBarrier transition_barrier;
        transition_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transition_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transition_barrier.oldLayout = vk::ImageLayout::eUndefined;
        transition_barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
        transition_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        transition_barrier.subresourceRange.levelCount = 1;
        transition_barrier.subresourceRange.layerCount = 1;
        for (auto image : images) {
            transition_barrier.image = image;
            barriers.emplace_back(transition_barrier);
        }
    }
    cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                               vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, barriers);
}

Swapchain::IndexSemaphore Swapchain::AcquireImage() {
    auto [result, index] = swapchain.getOwner().acquireNextImageKHR(*swapchain, std::numeric_limits<std::uint64_t>::max(),
                                   *image_available_semaphore, {});
    return {index, *image_available_semaphore};
}

void Swapchain::SwapImages(std::uint32_t index, vk::Semaphore swap_semaphore) {
        vk::PresentInfoKHR present_info;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &swap_semaphore;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &*swapchain;
        present_info.pImageIndices = &index;
        queue.presentKHR(present_info);
}

} // namespace nba::core
