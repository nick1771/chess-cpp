#pragma once

#include <vulkan/vulkan.hpp>

#include <any>

namespace Pandora::Implementation {

    vk::SurfaceKHR createVulkanSurface(vk::Instance instance, const std::any& windowHandle);
    void destroyVulkanSurface(vk::Instance instance, vk::SurfaceKHR surface);
}
