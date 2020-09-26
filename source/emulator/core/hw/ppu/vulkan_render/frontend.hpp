#pragma once

#include <common/vk_util.hpp>

namespace nba::core {
class VulkanFrontend {
public:
    virtual vk::Instance GetVulkanInstance() const = 0;
    virtual vk::SurfaceKHR GetDisplaySurface() const = 0;
    virtual vk::Extent2D GetDisplayDimensions() const = 0;

    std::uint64_t frame_count{0};
};
} // namespace nba::core
