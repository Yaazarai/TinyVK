#pragma once
#ifndef TINYVK_TINYVKUTILITIES
#define TINYVK_TINYVKUTILITIES
	#include "./TinyVulkan.hpp"

	namespace TINYVULKAN_NAMESPACE {
		#pragma region VULKAN_DEBUG_UTILITIES

		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
			auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
			if (func != VK_NULL_HANDLE) return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}

		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
			auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
			if (func != VK_NULL_HANDLE) func(instance, debugMessenger, pAllocator);
		}

		VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
			std::cerr << "TinyVulkan: Validation Layer: " << pCallbackData->pMessage << std::endl;
			return VK_FALSE;
		}

		#pragma endregion
		#pragma region VULKAN_DYNAMIC_RENDERING_FUNCTIONS

		PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingEXTKHR = VK_NULL_HANDLE;
		PFN_vkCmdEndRenderingKHR vkCmdEndRenderingEXTKHR = VK_NULL_HANDLE;
		PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetEXTKHR = VK_NULL_HANDLE;

		void vkCmdRenderingGetCallbacks(VkInstance instance) {
			vkCmdBeginRenderingEXTKHR = (PFN_vkCmdBeginRenderingKHR)vkGetInstanceProcAddr(instance, "vkCmdBeginRenderingKHR");
			vkCmdEndRenderingEXTKHR = (PFN_vkCmdEndRenderingKHR)vkGetInstanceProcAddr(instance, "vkCmdEndRenderingKHR");
			vkCmdPushDescriptorSetEXTKHR = (PFN_vkCmdPushDescriptorSetKHR)vkGetInstanceProcAddr(instance, "vkCmdPushDescriptorSetKHR");
		}

		VkResult vkCmdBeginRenderingEKHR(VkInstance instance, VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
			#if TVK_VALIDATION_LAYERS
				if (vkCmdBeginRenderingEXTKHR == VK_NULL_HANDLE)
					throw std::runtime_error("TinyVulkan: Failed to load VK_KHR_dynamic_rendering EXT function: PFN_vkCmdBeginRenderingKHR");
			#endif

			vkCmdBeginRenderingEXTKHR(commandBuffer, pRenderingInfo);
			return VK_SUCCESS;
		}

		VkResult vkCmdEndRenderingEKHR(VkInstance instance, VkCommandBuffer commandBuffer) {
			#if TVK_VALIDATION_LAYERS
				if (vkCmdEndRenderingEXTKHR == VK_NULL_HANDLE)
					throw std::runtime_error("TinyVulkan: Failed to load VK_KHR_dynamic_rendering EXT function: PFN_vkCmdEndRenderingKHR");
			#endif

			vkCmdEndRenderingEXTKHR(commandBuffer);
			return VK_SUCCESS;
		}

		VkResult vkCmdPushDescriptorSetEKHR(VkInstance instance, VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint, VkPipelineLayout layout, uint32_t set, uint32_t writeCount, const VkWriteDescriptorSet* pWriteSets) {
			#if TVK_VALIDATION_LAYERS
				if (vkCmdPushDescriptorSetEXTKHR == VK_NULL_HANDLE)
					throw std::runtime_error("TinyVulkan: Failed to load VK_KHR_dynamic_rendering EXT function: PFN_vkCmdPushDescriptorSetKHR");
			#endif

			vkCmdPushDescriptorSetEXTKHR(commandBuffer, bindPoint, layout, set, writeCount, pWriteSets);
			return VK_SUCCESS;
		}

		#pragma endregion

		/// <summary>List of valid Buffering Mode sizes.</summary>
		enum class TinyVkBufferingMode {
			DOUBLE = 2,
			TRIPLE = 3,
			QUADRUPLE = 4
		};

		/// <summary>Description of the SwapChain Rendering format.</summary>
		struct TinyVkSwapChainSupporter {
		public:
			VkSurfaceCapabilitiesKHR capabilities = {};
			std::vector<VkSurfaceFormatKHR> formats = {};
			std::vector<VkPresentModeKHR> presentModes = {};
		};

		/// <summary>Description of the Rendering Surface format.</summary>
		struct TinyVkSurfaceSupporter {
		public:
			VkFormat dataFormat = VK_FORMAT_B8G8R8A8_UNORM;
			VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			VkPresentModeKHR idealPresentMode = VK_PRESENT_MODE_FIFO_KHR;
		};

		class TinyVkRendererInterface {
		public:
			/// <summary>Alias call for easy-calls to: vkCmdBindVertexBuffers + vkCmdBindIndexBuffer.</summary>
			inline static void CmdBindGeometry(VkCommandBuffer cmdBuffer, const VkBuffer* vertexBuffers, const VkBuffer indexBuffer, const VkDeviceSize* offsets, const VkDeviceSize indexOffset = 0, uint32_t binding = 0, uint32_t bindingCount = 1) {
				vkCmdBindVertexBuffers(cmdBuffer, binding, bindingCount, vertexBuffers, offsets);
				vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, indexOffset, VK_INDEX_TYPE_UINT32);
			}

			/// <summary>Alias call for: vkCmdBindVertexBuffers.</summary>
			inline static void CmdBindGeometry(VkCommandBuffer cmdBuffer, const VkBuffer* vertexBuffers, const VkDeviceSize* offsets, uint32_t binding = 0, uint32_t bindingCount = 1) {
				vkCmdBindVertexBuffers(cmdBuffer, binding, bindingCount, vertexBuffers, offsets);
			}

			/// <summary>Alias call for: vkCmdBindIndexBuffers.</summary>
			inline static void CmdBindGeometry(VkCommandBuffer cmdBuffer, const VkBuffer indexBuffer, const VkDeviceSize indexOffset = 0, uint32_t binding = 0, uint32_t bindingCount = 1) {
				vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, indexOffset, VK_INDEX_TYPE_UINT32);
			}

			/// <summary>Alias call for: vkCmdBindVertexBuffers2.</summary>
			inline static void CmdBindGeometry(VkCommandBuffer cmdBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* vertexBuffers, const VkDeviceSize* vbufferOffsets, const VkDeviceSize* vbufferSizes, const VkDeviceSize* vbufferStrides = nullptr) {
					vkCmdBindVertexBuffers2(cmdBuffer, firstBinding, bindingCount, vertexBuffers, vbufferOffsets, vbufferSizes, vbufferStrides);
			}

			/// <summary>Alias call for vkCmdDraw (isIndexed = false) and vkCmdDrawIndexed (isIndexed = true).</summary>
			inline static void CmdDrawGeometry(VkCommandBuffer cmdBuffer, bool isIndexed = false, uint32_t instanceCount = 1, uint32_t firstInstance = 0, uint32_t vertexCount = 0, uint32_t vertexOffset = 0, uint32_t firstIndex = 0) {
				switch (isIndexed) {
					case true:
					vkCmdDrawIndexed(cmdBuffer, vertexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
					break;
					case false:
					vkCmdDraw(cmdBuffer, vertexCount, instanceCount, vertexOffset, firstInstance);
					break;
				}
			}

			/// <summary>Alias call for: vkCmdDrawIndexedIndirect.</summary>
			inline static void CmdDrawGeometryIndirect(VkCommandBuffer cmdBuffer, const VkBuffer drawParamBuffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
				vkCmdDrawIndexedIndirect(cmdBuffer, drawParamBuffer, offset, drawCount, stride);
			}

			/// <summary>Alias call for: vkCmdDrawIndexedIndirectCount.</summary>
			inline static void CmdDrawGeometryIndirect(VkCommandBuffer cmdBuffer, const VkBuffer drawParamBuffer, VkDeviceSize offset, const VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t drawCount, uint32_t maxDrawCount, uint32_t stride) {
				vkCmdDrawIndexedIndirectCount(cmdBuffer, drawParamBuffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
			}
		};
	}
#endif