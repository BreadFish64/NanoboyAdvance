/*
 * Copyright (C) 2020 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <cstring>

#include "frontend.hpp"
#include "vulkan_renderer.hpp"

namespace nba::core {

constexpr std::uint16_t VulkanRenderer::s_color_transparent;
constexpr int VulkanRenderer::s_wait_cycles[5];

VulkanRenderer::VulkanRenderer(Scheduler* scheduler, InterruptController* irq_controller, DMA* dma,
                               std::shared_ptr<Config> config)
    : nba::core::PPU(irq_controller), scheduler(scheduler), dma(dma),
      config(config), frontend{config->vulkan_frontend} {
    ASSERT(frontend, "Vulkan Frontend not initialized!");
    InitBlendTable();
    Reset();
    mmio.dispstat.ppu = this;

    vk.physical_device = frontend->GetVulkanInstance().enumeratePhysicalDevices()[0];
    {
        auto physical_properties = vk.physical_device.getProperties();
        LOG_INFO("Vulkan {}.{}", VK_VERSION_MAJOR(physical_properties.apiVersion),
                 VK_VERSION_MINOR(physical_properties.apiVersion));
        LOG_INFO("Device: {}", physical_properties.deviceName);
    }
    // Create device and get queue
    {
        vk.queue_family_index = Vulkan::GetQueueFamilyIndex(
            vk.physical_device, vk::QueueFlagBits::eTransfer | vk::QueueFlagBits::eGraphics);
        vk::DeviceQueueCreateInfo queue_create_info;
        queue_create_info.queueCount = 1;
        queue_create_info.queueFamilyIndex = vk.queue_family_index;
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

        vk.device = vk.physical_device.createDeviceUnique(device_info);
        vk.queue = vk.device->getQueue(vk.queue_family_index, 0);
    }
    // Create semaphore to signal blit completion
    blit.finished_semaphore = vk.device->createSemaphoreUnique({});
    // Fence to signal blit completion on the CPU side
    // so the CPU side copy doesn't occur during a blit
    staging.fence = vk.device->createFenceUnique({});
    {
        vk::CommandPoolCreateInfo cmd_pool_create_info;
        cmd_pool_create_info.queueFamilyIndex = vk.queue_family_index;
        vk.command_pool = vk.device->createCommandPoolUnique(cmd_pool_create_info);
    }
    // Command buffer to perform initial layout transitions
    vk::UniqueCommandBuffer initialization_command_buffer;
    {
        vk::CommandBufferAllocateInfo cmd_buffer_allocate_info;
        cmd_buffer_allocate_info.commandPool = *vk.command_pool;
        cmd_buffer_allocate_info.commandBufferCount = 1;
        initialization_command_buffer =
            std::move(vk.device->allocateCommandBuffersUnique(cmd_buffer_allocate_info)[0]);

        vk::CommandBufferBeginInfo command_begin_info;
        command_begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        initialization_command_buffer->begin(command_begin_info);
    }
    // Create image to upload the framebuffer data
    {
        vk::ImageCreateInfo staging_image_create_info;
        staging_image_create_info.imageType = vk::ImageType::e2D;
        staging_image_create_info.extent = vk::Extent3D{native_width, native_height, 1};
        staging_image_create_info.arrayLayers = 1;
        staging_image_create_info.mipLevels = 1;
        staging_image_create_info.format = vk::Format::eB8G8R8A8Unorm;
        staging_image_create_info.tiling = vk::ImageTiling::eLinear;
        staging_image_create_info.initialLayout = vk::ImageLayout::eUndefined;
        staging_image_create_info.sharingMode = vk::SharingMode::eExclusive;
        staging_image_create_info.usage = vk::ImageUsageFlagBits::eTransferSrc;

        staging.image = vk.device->createImageUnique(staging_image_create_info);

        {
            auto staging_memory_requirements =
                vk.device->getImageMemoryRequirements(*staging.image);
            vk::MemoryAllocateInfo staging_memory_allocation_info;
            staging_memory_allocation_info.allocationSize = staging_memory_requirements.size;
            staging_memory_allocation_info.memoryTypeIndex = Vulkan::GetMemoryType(
                vk.physical_device, staging_memory_requirements.memoryTypeBits,
                vk::MemoryPropertyFlagBits::eHostVisible |
                    vk::MemoryPropertyFlagBits::eHostCoherent);

            staging.memory = vk.device->allocateMemoryUnique(staging_memory_allocation_info);
            vk.device->bindImageMemory(*staging.image, *staging.memory, 0);
            staging.ptr = static_cast<std::uint32_t*>(vk.device->mapMemory(
                *staging.memory, 0, staging_memory_allocation_info.allocationSize));
        }

        // vk::ImageViewCreateInfo staging_view_create_info;
        // staging_view_create_info.image = *staging.image;
        // staging_view_create_info.viewType = vk::ImageViewType::e2D;
        // staging_view_create_info.format = staging_image_create_info.format;
        // staging_view_create_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        // staging_view_create_info.subresourceRange.levelCount =
        // staging_image_create_info.mipLevels;
        // staging_view_create_info.subresourceRange.layerCount =
        // staging_image_create_info.arrayLayers;

        // staging.view = vk_device->createImageViewUnique(staging_view_create_info);

        vk::ImageMemoryBarrier transition_barrier;
        transition_barrier.image = *staging.image;
        transition_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transition_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transition_barrier.oldLayout = vk::ImageLayout::eUndefined;
        transition_barrier.newLayout = vk::ImageLayout::eGeneral;
        transition_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        transition_barrier.subresourceRange.levelCount = 1;
        transition_barrier.subresourceRange.layerCount = 1;

        initialization_command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                                                       vk::PipelineStageFlagBits::eAllCommands, {},
                                                       {}, {}, transition_barrier);
    }
    swapchain = Swapchain{vk.physical_device,
                          *vk.device,
                          vk.queue_family_index,
                          vk.queue,
                          *initialization_command_buffer,
                          *frontend};
    initialization_command_buffer->end();
    {
        vk::SubmitInfo submit_info;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &*initialization_command_buffer;
        vk.queue.submit(submit_info, *staging.fence);
    }
    // Setup command buffers to blit to swapchain images
    {
        vk::CommandBufferAllocateInfo cmd_buffer_allocate_info;
        cmd_buffer_allocate_info.commandPool = *vk.command_pool;
        cmd_buffer_allocate_info.commandBufferCount = swapchain.GetImages().size();
        blit.command_buffers = vk.device->allocateCommandBuffersUnique(cmd_buffer_allocate_info);
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

        vk::ImageBlit image_blit;
        image_blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        image_blit.srcSubresource.layerCount = 1;
        image_blit.dstSubresource = image_blit.srcSubresource;
        image_blit.srcOffsets[0] = vk::Offset3D{0, 0, 0};
        image_blit.srcOffsets[1] = vk::Offset3D{native_width, native_height, 1};
        image_blit.dstOffsets[0] = vk::Offset3D{0, 0, 0};
        auto [window_width, window_height] = frontend->GetDisplayDimensions();
        image_blit.dstOffsets[1] = vk::Offset3D{static_cast<std::int32_t>(window_width), static_cast<std::int32_t>(window_height), 1};

        for (std::size_t idx{0}; idx < swapchain.GetImages().size(); ++idx) {
            auto& cmd_buffer = blit.command_buffers[idx];
            vk::Image present_image = swapchain.GetImages()[idx];

            dst_barrier.image = present_image;
            cmd_buffer->begin(vk::CommandBufferBeginInfo{});
            cmd_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                        vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barriers);
            cmd_buffer->blitImage(*staging.image, vk::ImageLayout::eTransferSrcOptimal,
                                  present_image, vk::ImageLayout::eTransferDstOptimal, image_blit,
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
    // Wait so we don't destroy the initialization command buffer while it's still active
    vk.queue.waitIdle();
}

VulkanRenderer::~VulkanRenderer() {
    vk.device->unmapMemory(*staging.memory);
}

void VulkanRenderer::Reset() {
    PPU::Reset();
    SetNextEvent(Phase::SCANLINE, 0);
}

void VulkanRenderer::Draw() {
    auto [image_index, acquire_image_semaphore] = swapchain.AcquireImage();
    vk.device->waitForFences(*staging.fence, true, std::numeric_limits<std::uint64_t>::max());
    vk.device->resetFences(*staging.fence);

    std::copy(output.begin(), output.end(), staging.ptr);
    {
        vk::SubmitInfo submit_info;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &*blit.command_buffers[image_index];
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &acquire_image_semaphore;
        vk::PipelineStageFlags stage{vk::PipelineStageFlagBits::eTransfer};
        submit_info.pWaitDstStageMask = &stage;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &*blit.finished_semaphore;
        vk.queue.submit(submit_info, *staging.fence);
    }
    swapchain.SwapImages(image_index, *blit.finished_semaphore);
    ++frontend->frame_count;
}

void VulkanRenderer::SetNextEvent(Phase phase, int cycles_late) {
    this->phase = phase;
    scheduler->Add(s_wait_cycles[static_cast<int>(phase)] - cycles_late, event_cb);
}

void VulkanRenderer::Tick(int cycles_late) {
    // TODO: get rid of the indirection and schedule the appropriate method directly.
    switch (phase) {
    case Phase::SCANLINE:
        OnScanlineComplete(cycles_late);
        break;
    case Phase::HBLANK_SEARCH:
        OnHblankSearchComplete(cycles_late);
        break;
    case Phase::HBLANK:
        OnHblankComplete(cycles_late);
        break;
    case Phase::VBLANK_SCANLINE:
        OnVblankScanlineComplete(cycles_late);
        break;
    case Phase::VBLANK_HBLANK:
        OnVblankHblankComplete(cycles_late);
        break;
    }
}

void VulkanRenderer::UpdateInternalAffineRegisters() {
    auto& bgx = mmio.bgx;
    auto& bgy = mmio.bgy;
    auto& mosaic = mmio.mosaic;

    if (mmio.vcount == 160 || mmio.dispcnt._mode_is_dirty) {
        mmio.dispcnt._mode_is_dirty = false;

        /* Reload internal affine registers */
        bgx[0]._current = bgx[0].initial;
        bgy[0]._current = bgy[0].initial;
        bgx[1]._current = bgx[1].initial;
        bgy[1]._current = bgy[1].initial;
    } else {
        for (int i = 0; i < 2; i++) {
            if (mmio.bgcnt[2 + i].mosaic_enable) {
                /* Vertical mosaic for affine-transformed layers. */
                if (mosaic.bg._counter_y == 0) {
                    bgx[i]._current += mosaic.bg.size_y * mmio.bgpb[i];
                    bgy[i]._current += mosaic.bg.size_y * mmio.bgpd[i];
                }
            } else {
                bgx[i]._current += mmio.bgpb[i];
                bgy[i]._current += mmio.bgpd[i];
            }
        }
    }
}

void VulkanRenderer::OnScanlineComplete(int cycles_late) {
    SetNextEvent(Phase::HBLANK_SEARCH, cycles_late);

    if (mmio.dispstat.hblank_irq_enable) {
        irq_controller->Raise(InterruptSource::HBlank);
    }

    RenderScanline();
}

void VulkanRenderer::OnHblankSearchComplete(int cycles_late) {
    SetNextEvent(Phase::HBLANK, cycles_late);

    dma->Request(DMA::Occasion::HBlank);
    if (mmio.vcount >= 2) {
        dma->Request(DMA::Occasion::Video);
    }
    mmio.dispstat.hblank_flag = 1;
}

void VulkanRenderer::OnHblankComplete(int cycles_late) {
    auto& vcount = mmio.vcount;
    auto& dispstat = mmio.dispstat;
    auto& mosaic = mmio.mosaic;

    dispstat.hblank_flag = 0;
    vcount++;
    CheckVerticalCounterIRQ();

    if (vcount == 160) {
        Draw();

        SetNextEvent(Phase::VBLANK_SCANLINE, cycles_late);
        dma->Request(DMA::Occasion::VBlank);
        dispstat.vblank_flag = 1;

        if (dispstat.vblank_irq_enable) {
            irq_controller->Raise(InterruptSource::VBlank);
        }

        /* Reset vertical mosaic counters */
        mosaic.bg._counter_y = 0;
        mosaic.obj._counter_y = 0;
    } else {
        /* Advance vertical background mosaic counter */
        if (++mosaic.bg._counter_y == mosaic.bg.size_y) {
            mosaic.bg._counter_y = 0;
        }

        /* Advance vertical OBJ mosaic counter */
        if (++mosaic.obj._counter_y == mosaic.obj.size_y) {
            mosaic.obj._counter_y = 0;
        }

        SetNextEvent(Phase::SCANLINE, cycles_late);
    }

    UpdateInternalAffineRegisters();
}

void VulkanRenderer::OnVblankScanlineComplete(int cycles_late) {
    auto& dispstat = mmio.dispstat;

    SetNextEvent(Phase::VBLANK_HBLANK, cycles_late);
    dispstat.hblank_flag = 1;

    if (mmio.vcount < 162) {
        dma->Request(DMA::Occasion::Video);
    } else if (mmio.vcount == 162) {
        dma->StopVideoXferDMA();
    }

    if (dispstat.hblank_irq_enable) {
        irq_controller->Raise(InterruptSource::HBlank);
    }
}

void VulkanRenderer::OnVblankHblankComplete(int cycles_late) {
    auto& vcount = mmio.vcount;
    auto& dispstat = mmio.dispstat;

    dispstat.hblank_flag = 0;

    if (vcount == 227) {
        vcount = 0;
        SetNextEvent(Phase::SCANLINE, cycles_late);
    } else {
        SetNextEvent(Phase::VBLANK_SCANLINE, cycles_late);
        if (vcount == 226) {
            dispstat.vblank_flag = 0;
        }
        vcount++;
    }

    CheckVerticalCounterIRQ();
}

} // namespace nba::core
