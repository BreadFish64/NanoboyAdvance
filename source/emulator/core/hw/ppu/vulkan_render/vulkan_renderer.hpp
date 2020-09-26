/*
 * Copyright (C) 2020 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <emulator/config/config.hpp>
#include <emulator/core/hw/dma.hpp>
#include <emulator/core/hw/ppu/ppu.hpp>
#include <emulator/core/hw/ppu/registers.hpp>
#include <emulator/core/scheduler.hpp>

#include "swapchain.hpp"

namespace nba::core {

class VulkanFrontend;

class VulkanRenderer final : public nba::core::PPU {
public:
    static constexpr std::uint32_t native_width = 240, native_height = 160;

    VulkanRenderer(Scheduler* scheduler, InterruptController* irq_controller, DMA* dma,
                   std::shared_ptr<Config> config);
    virtual ~VulkanRenderer() override;

    virtual void Reset() override;

private:
    friend struct DisplayStatus;

    enum class Phase {
        SCANLINE = 0,
        HBLANK_SEARCH = 1,
        HBLANK = 2,
        VBLANK_SCANLINE = 3,
        VBLANK_HBLANK = 4
    };

    enum ObjAttribute { OBJ_IS_ALPHA = 1, OBJ_IS_WINDOW = 2 };

    enum ObjectMode { OBJ_NORMAL = 0, OBJ_SEMI = 1, OBJ_WINDOW = 2, OBJ_PROHIBITED = 3 };

    enum Layer {
        LAYER_BG0 = 0,
        LAYER_BG1 = 1,
        LAYER_BG2 = 2,
        LAYER_BG3 = 3,
        LAYER_OBJ = 4,
        LAYER_SFX = 5,
        LAYER_BD = 5
    };

    enum Enable {
        ENABLE_BG0 = 0,
        ENABLE_BG1 = 1,
        ENABLE_BG2 = 2,
        ENABLE_BG3 = 3,
        ENABLE_OBJ = 4,
        ENABLE_WIN0 = 5,
        ENABLE_WIN1 = 6,
        ENABLE_OBJWIN = 7
    };

    static auto ConvertColor(std::uint16_t color) -> std::uint32_t;

    void Tick(int cycles_late);

    void UpdateInternalAffineRegisters();

    void SetNextEvent(Phase phase, int cycles_late);
    void OnScanlineComplete(int cycles_late);
    void OnHblankSearchComplete(int cycles_late);
    void OnHblankComplete(int cycles_late);
    void OnVblankScanlineComplete(int cycles_late);
    void OnVblankHblankComplete(int cycles_late);

    void RenderScanline();
    void RenderLayerText(int id);
    void RenderLayerAffine(int id);
    void RenderLayerBitmap1();
    void RenderLayerBitmap2();
    void RenderLayerBitmap3();
    void RenderLayerOAM(bool bitmap_mode);
    void RenderWindow(int id);

    void ComposeScanline(int bg_min, int bg_max);
    void InitBlendTable();
    void Blend(std::uint16_t& target1, std::uint16_t target2, BlendControl::Effect sfx);

    void Draw();

#include <emulator/core/hw/ppu/helper.inl>

    std::shared_ptr<VulkanFrontend> frontend;

    struct {
        vk::PhysicalDevice physical_device;
        std::uint32_t queue_family_index;
        vk::Queue queue;
        vk::UniqueDevice device;
        vk::UniqueCommandPool command_pool;
    } vk;
    struct {
        vk::UniqueImage image;
        vk::UniqueDeviceMemory memory;
        // vk::UniqueImageView view;
        vk::ImageLayout layout{};
        std::uint32_t* ptr{};
        vk::UniqueFence fence;
    } staging;
    struct {
        std::vector<vk::UniqueCommandBuffer> command_buffers;
        vk::UniqueSemaphore finished_semaphore;
    } blit;

    Swapchain swapchain;

    Scheduler* scheduler;
    DMA* dma;
    std::shared_ptr<Config> config;
    std::function<void(int)> event_cb = [this](int cycles_late) { this->Tick(cycles_late); };

    std::uint16_t buffer_bg[4][native_width];

    struct ObjectPixel {
        std::uint16_t color;
        std::uint8_t priority;
        unsigned alpha : 1;
        unsigned window : 1;
    } buffer_obj[native_width];

    bool line_contains_alpha_obj;

    bool buffer_win[2][native_width];
    bool window_scanline_enable[2];

    std::array<std::uint32_t, native_width * native_height> output;

    Phase phase;

    std::uint8_t blend_table[17][17][32][32];

    static constexpr std::uint16_t s_color_transparent = 0x8000;
    static constexpr int s_wait_cycles[5] = {960, 46, 226, 1006, 226};
    static const int s_obj_size[4][4][2];
};

} // namespace nba::core
