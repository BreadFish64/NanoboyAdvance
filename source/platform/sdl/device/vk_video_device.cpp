#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <emulator/config/config.hpp>

#include "vk_video_device.hpp"

SDL2_VK_VideoDevice::SDL2_VK_VideoDevice(const nba::Config& config) : config{config} {
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
    vk_physical_device = vk_instance->enumeratePhysicalDevices()[0];
    {
        auto physical_properties = vk_physical_device.getProperties();
        LOG_INFO("Vulkan {}.{}", VK_VERSION_MAJOR(physical_properties.apiVersion),
                 VK_VERSION_MINOR(physical_properties.apiVersion));
        LOG_INFO("Device: {}", physical_properties.deviceName);
    }
    // Create device and get queue
    {
        queue_family_index = Vulkan::GetQueueFamilyIndex(
            vk_physical_device, vk::QueueFlagBits::eTransfer | vk::QueueFlagBits::eGraphics);
        vk::DeviceQueueCreateInfo queue_create_info;
        queue_create_info.queueCount = 1;
        queue_create_info.queueFamilyIndex = queue_family_index;
        static constexpr float priority = 1.0;
        queue_create_info.pQueuePriorities = &priority;
        vk::DeviceCreateInfo device_info;
        device_info.setQueueCreateInfoCount(1);
        device_info.setPQueueCreateInfos(&queue_create_info);

        constexpr std::array device_extensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };
        device_info.enabledExtensionCount = device_extensions.size();
        device_info.ppEnabledExtensionNames = device_extensions.data();

        vk_device = vk_physical_device.createDeviceUnique(device_info);
        vk_queue = vk_device->getQueue(queue_family_index, 0);
    }
    ASSERT(vk_physical_device.getSurfaceSupportKHR(queue_family_index, *vk_window_surface),
           "Window surface is not supported on this queue family");
    {
        vk::SwapchainCreateInfoKHR swapchain_create_info;
        swapchain_create_info.surface = *vk_window_surface;
        auto surface_capabilities =
            vk_physical_device.getSurfaceCapabilitiesKHR(*vk_window_surface);

        // Choose swapchain image count
        swapchain_create_info.minImageCount = surface_capabilities.minImageCount + 1;
        if (surface_capabilities.maxImageCount != 0 &&
            swapchain_create_info.minImageCount > surface_capabilities.maxImageCount) {
            swapchain_create_info.minImageCount = surface_capabilities.maxImageCount;
        }

        // TODO: don't assume BGRA8_UNORM
        {
            auto surface_formats = vk_physical_device.getSurfaceFormatsKHR(*vk_window_surface);
            auto surface_format =
                std::find_if(surface_formats.begin(), surface_formats.end(),
                             [](const vk::SurfaceFormatKHR& format) {
                                 return format.format == vk::Format::eB8G8R8A8Unorm;
                             });
            ASSERT(surface_format != surface_formats.end(), "B8G8R8A8Unorm Surface unsupported");
            vk_window_surface_format = vk::Format::eB8G8R8A8Unorm;
            swapchain_create_info.imageFormat = surface_format->format;
            swapchain_create_info.imageColorSpace = surface_format->colorSpace;
        }
        // Prefer Mailbox mode for lower latency, otherwise default to FIFO
        {
            auto present_modes = vk_physical_device.getSurfacePresentModesKHR(*vk_window_surface);
            swapchain_create_info.presentMode =
                std::find(present_modes.begin(), present_modes.end(),
                          vk::PresentModeKHR::eMailbox) == present_modes.end()
                    ? vk::PresentModeKHR::eFifo
                    : vk::PresentModeKHR::eMailbox;
        }
        // Recommended way to get window image size for SDL2
        // https://wiki.libsdl.org/SDL_Vulkan_GetDrawableSize
        {
            SDL_Vulkan_GetDrawableSize(window.get(), &window_width, &window_height);
            swapchain_create_info.imageExtent.width = window_width;
            swapchain_create_info.imageExtent.height = window_height;
        }
        swapchain_create_info.imageArrayLayers = 1;
        swapchain_create_info.imageUsage =
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment;
        swapchain_create_info.imageSharingMode = vk::SharingMode::eExclusive;
        swapchain_create_info.preTransform = surface_capabilities.currentTransform;
        swapchain_create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapchain_create_info.clipped = VK_TRUE;

        vk_swapchain = vk_device->createSwapchainKHRUnique(swapchain_create_info);
    }
    vk_swapchain_images = vk_device->getSwapchainImagesKHR(*vk_swapchain);
    // Create image views for all of the swapchain images
    //{
    //    vk::ImageViewCreateInfo swapchain_image_view_create_info;
    //    swapchain_image_view_create_info.viewType = vk::ImageViewType::e2D;
    //    swapchain_image_view_create_info.format = vk_window_surface_format;
    //    swapchain_image_view_create_info.subresourceRange.aspectMask =
    //        vk::ImageAspectFlagBits::eColor;
    //    swapchain_image_view_create_info.subresourceRange.levelCount = 1;
    //    swapchain_image_view_create_info.subresourceRange.layerCount = 1;

    //    for (auto image : vk_swapchain_images) {
    //        swapchain_image_view_create_info.image = image;
    //        vk_swapchain_image_views.emplace_back(
    //            vk_device->createImageViewUnique(swapchain_image_view_create_info));
    //    }
    //}
    // Create image to upload the framebuffer data
    {
        vk::ImageCreateInfo staging_image_create_info;
        staging_image_create_info.imageType = vk::ImageType::e2D;
        staging_image_create_info.extent = vk::Extent3D{native_width, native_height, 1};
        staging_image_create_info.arrayLayers = 1;
        staging_image_create_info.mipLevels = 1;
        staging_image_create_info.format = vk::Format::eB8G8R8A8Unorm;
        staging_image_create_info.tiling = vk::ImageTiling::eLinear;
        staging_image_create_info.initialLayout = staging.layout;
        staging_image_create_info.sharingMode = vk::SharingMode::eExclusive;
        staging_image_create_info.usage = vk::ImageUsageFlagBits::eTransferSrc;

        staging.image = vk_device->createImageUnique(staging_image_create_info);

        {
            auto staging_memory_requirements =
                vk_device->getImageMemoryRequirements(*staging.image);
            vk::MemoryAllocateInfo staging_memory_allocation_info;
            staging_memory_allocation_info.allocationSize = staging_memory_requirements.size;
            staging_memory_allocation_info.memoryTypeIndex = Vulkan::GetMemoryType(
                vk_physical_device, staging_memory_requirements.memoryTypeBits,
                vk::MemoryPropertyFlagBits::eHostVisible |
                    vk::MemoryPropertyFlagBits::eHostCoherent);

            staging.memory = vk_device->allocateMemoryUnique(staging_memory_allocation_info);
            vk_device->bindImageMemory(*staging.image, *staging.memory, 0);
            staging.ptr = static_cast<std::uint32_t*>(vk_device->mapMemory(
                *staging.memory, 0, staging_memory_allocation_info.allocationSize));
        }

        // vk::ImageViewCreateInfo staging_view_create_info;
        // staging_view_create_info.image = *staging.image;
        // staging_view_create_info.viewType = vk::ImageViewType::e2D;
        // staging_view_create_info.format = staging_image_create_info.format;
        // staging_view_create_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        // staging_view_create_info.subresourceRange.levelCount =
        // staging_image_create_info.mipLevels; staging_view_create_info.subresourceRange.layerCount
        // =
        //    staging_image_create_info.arrayLayers;

        // staging.view = vk_device->createImageViewUnique(staging_view_create_info);
    }
    // Sync for presentation
    image_available_semaphore = vk_device->createSemaphoreUnique({});
    blit_finished_semaphore = vk_device->createSemaphoreUnique({});
    // CPU side sync for staging
    draw_fence = vk_device->createFenceUnique({});
    {
        vk::CommandPoolCreateInfo cmd_pool_create_info;
        cmd_pool_create_info.queueFamilyIndex = queue_family_index;
        cmd_pool = vk_device->createCommandPoolUnique(cmd_pool_create_info);
    }
    // Perform initial layout transition
    {
        vk::CommandBufferAllocateInfo cmd_buffer_allocate_info;
        cmd_buffer_allocate_info.commandPool = *cmd_pool;
        cmd_buffer_allocate_info.commandBufferCount = 1;
        auto cmd_buffer =
            std::move(vk_device->allocateCommandBuffersUnique(cmd_buffer_allocate_info)[0]);

        std::vector<vk::ImageMemoryBarrier> barriers;
        {
            vk::ImageMemoryBarrier barrier;
            barrier.image = *staging.image;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.oldLayout = vk::ImageLayout::eUndefined;
            barrier.newLayout = vk::ImageLayout::eGeneral;
            barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;

            barriers.emplace_back(barrier);

            barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
            for (auto image : vk_swapchain_images) {
                barrier.image = image;
                barriers.emplace_back(barrier);
            }
        }

        vk::CommandBufferBeginInfo cmd_buffer_begin;
        cmd_buffer_begin.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        cmd_buffer->begin(cmd_buffer_begin);
        cmd_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                                    vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, barriers);
        cmd_buffer->end();

        vk::SubmitInfo submit_info;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &*cmd_buffer;
        vk_queue.submit(submit_info, *draw_fence);
        vk_queue.waitIdle();
    }
    // Setup command buffers to blit to swapchain images
    {
        vk::CommandBufferAllocateInfo cmd_buffer_allocate_info;
        cmd_buffer_allocate_info.commandPool = *cmd_pool;
        cmd_buffer_allocate_info.commandBufferCount = vk_swapchain_images.size();
        blit_cmd_buffers = vk_device->allocateCommandBuffersUnique(cmd_buffer_allocate_info);
    }
    {
        std::array<vk::ImageMemoryBarrier, 2> barriers;
        auto& src_barrier = barriers[0];
        src_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        src_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        src_barrier.oldLayout = vk::ImageLayout::eGeneral;
        src_barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        src_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        src_barrier.subresourceRange.levelCount = 1;
        src_barrier.subresourceRange.layerCount = 1;
        auto& dst_barrier = barriers[1];
        dst_barrier = src_barrier;
        dst_barrier.oldLayout = vk::ImageLayout::ePresentSrcKHR;
        dst_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
        dst_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;

        src_barrier.image = *staging.image;

        vk::ImageBlit blit;
        blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.srcSubresource.layerCount = 1;
        blit.dstSubresource = blit.srcSubresource;
        blit.srcOffsets[0] = vk::Offset3D{0, 0, 0};
        blit.srcOffsets[1] = vk::Offset3D{native_width, native_height, 1};
        blit.dstOffsets[0] = vk::Offset3D{0, 0, 0};
        blit.dstOffsets[1] = vk::Offset3D{window_width, window_height, 1};

        for (std::size_t idx{0}; idx < vk_swapchain_images.size(); ++idx) {
            auto& cmd_buffer = blit_cmd_buffers[idx];
            vk::Image present_image = vk_swapchain_images[idx];

            dst_barrier.image = present_image;
            cmd_buffer->begin(vk::CommandBufferBeginInfo{});
            cmd_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                        vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barriers);
            cmd_buffer->blitImage(*staging.image, vk::ImageLayout::eTransferSrcOptimal,
                                  present_image, vk::ImageLayout::eTransferDstOptimal, blit,
                                  vk::Filter::eNearest);

            std::swap(src_barrier.oldLayout, src_barrier.newLayout);
            std::swap(dst_barrier.oldLayout, dst_barrier.newLayout);

            cmd_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                        vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barriers);
            cmd_buffer->end();

            std::swap(src_barrier.oldLayout, src_barrier.newLayout);
            std::swap(dst_barrier.oldLayout, dst_barrier.newLayout);
        }
    }
}

SDL2_VK_VideoDevice::~SDL2_VK_VideoDevice() {
    vk_device->unmapMemory(*staging.memory);
}

inline void SDL2_VK_VideoDevice::Draw(std::uint32_t* buffer) {
    vk_device->waitForFences(*draw_fence, true, std::numeric_limits<std::uint64_t>::max());
    vk_device->resetFences(*draw_fence);

    std::copy_n(buffer, native_width * native_height, staging.ptr);

    auto [acquire_image_result, image_index] = vk_device->acquireNextImageKHR(
        *vk_swapchain, std::numeric_limits<std::uint64_t>::max(), *image_available_semaphore, {});

    {
        vk::SubmitInfo submit_info;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &*blit_cmd_buffers[image_index];
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &*image_available_semaphore;
        vk::PipelineStageFlags stage{vk::PipelineStageFlagBits::eTransfer};
        submit_info.pWaitDstStageMask = &stage;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &*blit_finished_semaphore;
        vk_queue.submit(submit_info, *draw_fence);
    }
    {
        vk::PresentInfoKHR present_info;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &*blit_finished_semaphore;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &*vk_swapchain;
        present_info.pImageIndices = &image_index;
        vk_queue.presentKHR(present_info);
    }
}
