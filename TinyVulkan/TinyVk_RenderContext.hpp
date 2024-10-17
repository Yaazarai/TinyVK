#pragma once
#ifndef DESKTOPSHADERS_TINYVKRENDERCONTEXT
#define DESKTOPSHADERS_TINYVKRENDERCONTEXT

    #include "./TinyVulkan.hpp"

    namespace TINYVULKAN_NAMESPACE {
        /// @brief Graphics pipeline render context for the Image and Swapchain renderers.
        class TinyVkRenderContext : public TinyVkRendererInterface {
        public:
            tinyvk::TinyVkVulkanDevice& vkdevice;
            tinyvk::TinyVkCommandPool& commandPool;
            tinyvk::TinyVkGraphicsPipeline& graphicsPipeline;

            TinyVkRenderContext(TinyVkVulkanDevice& vkdevice, TinyVkCommandPool& commandPool, TinyVkGraphicsPipeline& graphicsPipeline)
                : vkdevice(vkdevice), commandPool(commandPool), graphicsPipeline(graphicsPipeline) {}
        };
    }

#endif