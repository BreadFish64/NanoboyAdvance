/*
 * Copyright (C) 2020 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include "vulkan_renderer.hpp"

namespace nba::core {

void VulkanRenderer::RenderLayerBitmap1() {
  AffineRenderLoop(0, 240, 160, [&](int line_x, int x, int y) {
    int index = y * 480 + x * 2;
    
    buffer_bg[2][line_x] = (vram[index + 1] << 8) | vram[index];
  });
}

void VulkanRenderer::RenderLayerBitmap2() {  
  auto frame = mmio.dispcnt.frame * 0xA000;
  
  AffineRenderLoop(0, 240, 160, [&](int line_x, int x, int y) {
    int index = frame + y * 240 + x;
    
    buffer_bg[2][line_x] = ReadPalette(0, vram[index]);
  });
}

void VulkanRenderer::RenderLayerBitmap3() {
  auto frame = mmio.dispcnt.frame * 0xA000;
  
  AffineRenderLoop(0, 160, 128, [&](int line_x, int x, int y) {
    int index = frame + y * 320 + x * 2;
    
    buffer_bg[2][line_x] = (vram[index + 1] << 8) | vram[index];
  });
}

} // namespace nba::core
