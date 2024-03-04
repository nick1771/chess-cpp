#pragma once

#include <vulkan/vulkan.hpp>

namespace Pandora::Implementation {

    struct VulkanExtensionDispatch {
        PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT{};
        PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT{};

        VulkanExtensionDispatch() = default;
        VulkanExtensionDispatch(vk::Instance instance);

        uint32_t getVkHeaderVersion() const;
    };
}
