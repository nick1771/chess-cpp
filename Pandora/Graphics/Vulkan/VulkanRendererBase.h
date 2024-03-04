#pragma once

#include "Pandora/Mathematics/Matrix.h"
#include "Pandora/Mathematics/Vector.h"
#include "Pandora/Windowing/Window.h"
#include "Pandora/Pandora.h"

#include "VulkanAllocator.h"
#include "VulkanExtensionDispatch.h"
#include "VulkanResourceAllocator.h"
#include "VulkanDescriptorCache.h"
#include "VulkanPipeline.h"

namespace Pandora::Implementation {

    class VulkanRendererBase {
    public:
        virtual void initialize(const Window& window);
        virtual void resize(u32 width, u32 height);
    protected:
        VulkanExtensionDispatch _extensionDispatch{};
        VulkanResourceAllocator _resourceAllocator{};
        VulkanDescriptorCache _descriptorCache{};

        vk::DebugUtilsMessengerEXT _debugUtilsMessenger{};
        vk::Instance _instance{};

        vk::PhysicalDevice _physicalDevice{};
        vk::Device _logicalDevice{};

        u32 _graphicsQueueIndex{};
        vk::Queue _graphicsQueue{};

        vk::Extent2D _viewport{};
        vk::SurfaceKHR _surface{};
        vk::SwapchainKHR _swapchain{};
        std::vector<vk::Image> _swapchainImages{};
        std::vector<vk::UniqueImageView> _swapchainViews{};

        bool _isSuspended{};
        u32 _renderTargetId{};
    };
}
