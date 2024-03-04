#include "VulkanExtensionDispatch.h"

#define LOAD_FUNCTION(name) reinterpret_cast<PFN_##name>(instance.getProcAddr(#name)); 

namespace Pandora::Implementation {

    VulkanExtensionDispatch::VulkanExtensionDispatch(vk::Instance instance) {
        vkCreateDebugUtilsMessengerEXT = LOAD_FUNCTION(vkCreateDebugUtilsMessengerEXT);
        vkDestroyDebugUtilsMessengerEXT = LOAD_FUNCTION(vkDestroyDebugUtilsMessengerEXT);
    }

    uint32_t VulkanExtensionDispatch::getVkHeaderVersion() const {
        return vk::HeaderVersion;
    }
}
