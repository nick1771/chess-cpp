#include "VulkanSurface.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <vulkan/vulkan_win32.h>

namespace Pandora::Implementation {

    vk::SurfaceKHR createVulkanSurface(vk::Instance instance, const std::any& windowHandle) {
        auto surfaceCreateInfo = VkWin32SurfaceCreateInfoKHR{};
        surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceCreateInfo.hwnd = std::any_cast<HWND>(windowHandle);
        surfaceCreateInfo.hinstance = GetModuleHandleW(nullptr);

        VkSurfaceKHR surfaceHandle{};
        auto createResult = vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surfaceHandle);
        vk::resultCheck(static_cast<vk::Result>(createResult), "vkCreateWin32SurfaceKHR");

        return static_cast<vk::SurfaceKHR>(surfaceHandle);
    }
}
