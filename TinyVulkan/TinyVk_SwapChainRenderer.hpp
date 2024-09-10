#pragma once
#ifndef TINYVK_TINYVKSWAPCHAINRENDERER
#define TINYVK_TINYVKSWAPCHAINRENDERER
	#include "./TinyVulkan.hpp"

	namespace TINYVULKAN_NAMESPACE {
		/// 
		/// RENDERING PARADIGM:
		///		TinyVkImageRenderer is for rendering directly to a VkImage render target for offscreen rendering.
		///		Call RenderExecute(mutex[optional], preRecordedCommandBuffer[optiona]) to render to the image.
		///			You may pass a pre-recorded command buffer ready to go, or retrieve a command buffer from
		///			the underlying command pool queue and build your command buffer with a render event (onRenderEvent).
		///				onRenderEvent += callback<VkCommandBuffer>(...);
		///				
		///			If you use a render event, the command buffer will be returned to the command pool queue after execution.
		///		
		///		TinyVkSwapChainRenderer is for rendering directly to the SwapChain for onscreen rendering.
		///		Call RenderExecute(mutex[optional]) to render to the swap chain image.
		///			All swap chain rendering is done via render events and does not accept pre-recorded command buffers.
		///			The command buffer will be returned to the command pool queue after execution.
		///	
		///	Why? This rendering paradigm allows the swap chain to effectively manage and synchronize its own resources
		///	minimizing screen rendering errors or validation layer errors.
		/// 
		/// Both the TinyVkImageRenderer and TinyVkSwapChainRenderer contain their own managed depthImages which will
		/// be optionally created and utilized if their underlying graphics pipeline supports depth testing.
		/// 
		
		/// <summary>Onscreen Rendering (Render/Present-To-Screen Model): Render to SwapChain.</summary>
		class TinyVkSwapChainRenderer : public TinyVkRendererInterface, TinyVkDisposable {
		private:
			std::timed_mutex swapChainMutex;
			TinyVkSurfaceSupporter presentDetails;
			VkSwapchainKHR swapChain = VK_NULL_HANDLE;
			VkFormat imageFormat;
			VkExtent2D imageExtent;
			VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			TinyVkBufferingMode bufferingMode;
			std::vector<VkImage> images;
			std::vector<VkImageView> imageViews;

			/// <summary>Create the Vulkan surface swap-chain images and imageviews.</summary>
			void CreateSwapChainImages(uint32_t width = 0, uint32_t height = 0) {
				TinyVkSwapChainSupporter swapChainSupport = QuerySwapChainSupport(renderContext.vkdevice.GetPhysicalDevice());
				VkSurfaceFormatKHR surfaceFormat = QuerySwapSurfaceFormat(swapChainSupport.formats);
				VkPresentModeKHR presentMode = QuerySwapPresentMode(swapChainSupport.presentModes);
				VkExtent2D extent = QuerySwapExtent(swapChainSupport.capabilities);
				uint32_t imageCount = std::min(swapChainSupport.capabilities.maxImageCount, std::max(swapChainSupport.capabilities.minImageCount, static_cast<uint32_t>(bufferingMode)));

				if (width != 0 && height != 0) {
					extent = {
						std::min(std::max((uint32_t)width, swapChainSupport.capabilities.minImageExtent.width), swapChainSupport.capabilities.maxImageExtent.width),
						std::min(std::max((uint32_t)height, swapChainSupport.capabilities.minImageExtent.height), swapChainSupport.capabilities.maxImageExtent.height)
					};
				} else {
					extent = QuerySwapExtent(swapChainSupport.capabilities);
				}

				if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
					imageCount = swapChainSupport.capabilities.maxImageCount;

				VkSwapchainCreateInfoKHR createInfo{};
				createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
				createInfo.surface = renderContext.vkdevice.GetPresentSurface();
				createInfo.minImageCount = imageCount;
				createInfo.imageFormat = surfaceFormat.format;
				createInfo.imageColorSpace = surfaceFormat.colorSpace;
				createInfo.imageExtent = extent;
				createInfo.imageArrayLayers = 1; // Change when developing VR or other 3D stereoscopic applications
				createInfo.imageUsage = imageUsage;

				TinyVkQueueFamily indices = renderContext.vkdevice.FindQueueFamilies();
				uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

				if (indices.graphicsFamily != indices.presentFamily) {
					createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
					createInfo.queueFamilyIndexCount = 2;
					createInfo.pQueueFamilyIndices = queueFamilyIndices;
				} else {
					createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
					createInfo.queueFamilyIndexCount = 0; // Optional
					createInfo.pQueueFamilyIndices = VK_NULL_HANDLE; // Optional
				}

				createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
				createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
				createInfo.presentMode = presentMode;
				createInfo.clipped = VK_TRUE;
				createInfo.oldSwapchain = swapChain;

				if (vkCreateSwapchainKHR(renderContext.vkdevice.GetLogicalDevice(), &createInfo, VK_NULL_HANDLE, &swapChain) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to create swap chain!");

				vkGetSwapchainImagesKHR(renderContext.vkdevice.GetLogicalDevice(), swapChain, &imageCount, VK_NULL_HANDLE);
				images.resize(imageCount);
				vkGetSwapchainImagesKHR(renderContext.vkdevice.GetLogicalDevice(), swapChain, &imageCount, images.data());

				imageFormat = surfaceFormat.format;
				imageExtent = extent;
			}

			/// <summary>Create the image views for rendering to images (including those in the swap-chain).</summary>
			void CreateSwapChainImageViews(VkImageViewCreateInfo* createInfoEx = VK_NULL_HANDLE) {
				imageViews.resize(images.size());

				for (size_t i = 0; i < images.size(); i++) {
					VkImageViewCreateInfo createInfo{};

					if (createInfoEx == VK_NULL_HANDLE) {
						createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
						createInfo.image = images[i];
						createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
						createInfo.format = imageFormat;

						createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
						createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
						createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
						createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

						createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
						createInfo.subresourceRange.baseMipLevel = 0;
						createInfo.subresourceRange.levelCount = 1;
						createInfo.subresourceRange.baseArrayLayer = 0;
						createInfo.subresourceRange.layerCount = 1;
					} else { createInfo = *createInfoEx; }

					if (vkCreateImageView(renderContext.vkdevice.GetLogicalDevice(), &createInfo, VK_NULL_HANDLE, &imageViews[i]) != VK_SUCCESS)
						throw std::runtime_error("TinyVulkan: Failed to create swap chain image views!");
				}
			}

			/// <summary>Create the Vulkan surface swapchain.</summary>
			void CreateSwapChain(uint32_t width = 0, uint32_t height = 0) {
				CreateSwapChainImages(width, height);
				CreateSwapChainImageViews();
			}

			/// <summary>Checks the VkPhysicalDevice for swap-chain availability.</summary>
			TinyVkSwapChainSupporter QuerySwapChainSupport(VkPhysicalDevice device) {
				TinyVkSwapChainSupporter details;

				uint32_t formatCount;
				VkSurfaceKHR windowSurface = renderContext.vkdevice.GetPresentSurface();
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, windowSurface, &formatCount, VK_NULL_HANDLE);
				vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, windowSurface, &details.capabilities);

				if (formatCount > 0) {
					details.formats.resize(formatCount);
					vkGetPhysicalDeviceSurfaceFormatsKHR(device, windowSurface, &formatCount, details.formats.data());
				}

				uint32_t presentModeCount;
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, windowSurface, &presentModeCount, VK_NULL_HANDLE);

				if (presentModeCount != 0) {
					details.presentModes.resize(presentModeCount);
					vkGetPhysicalDeviceSurfacePresentModesKHR(device, windowSurface, &presentModeCount, details.presentModes.data());
				}

				return details;
			}

			/// <summary>Gets the swap-chain surface format.</summary>
			VkSurfaceFormatKHR QuerySwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
				for (const auto& availableFormat : availableFormats)
					if (availableFormat.format == presentDetails.dataFormat && availableFormat.colorSpace == presentDetails.colorSpace)
						return availableFormat;

				return availableFormats[0];
			}

			/// <summary>Select the swap-chains active present mode.</summary>
			VkPresentModeKHR QuerySwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
				for (const auto& availablePresentMode : availablePresentModes)
					if (availablePresentMode == presentDetails.idealPresentMode)
						return availablePresentMode;

				return VK_PRESENT_MODE_FIFO_KHR;
			}

			/// <summary>Select swap-chain extent (swap-chain surface resolution).</summary>
			VkExtent2D QuerySwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
				int width, height;
				window.OnFrameBufferReSizeCallback(width, height);

				VkExtent2D extent = {
					std::min(std::max((uint32_t)width, capabilities.minImageExtent.width), capabilities.maxImageExtent.width),
					std::min(std::max((uint32_t)height, capabilities.minImageExtent.height), capabilities.maxImageExtent.height)
				};

				extent.width = std::max(1u, extent.width);
				extent.height = std::max(1u, extent.height);
				return extent;
			}
			
			std::vector<VkSemaphore> imageAvailableSemaphores;
			std::vector<VkSemaphore> renderFinishedSemaphores;
			std::vector<VkFence> inFlightFences;
			std::vector<TinyVkImage*> optionalDepthImages;
			std::vector<TinyVkCommandPool*> commandPools;
			std::vector<VkExtent2D> frameRenderSizes;
			TinyVkCommandPool* depthImagePool;
			TinyVkRenderContext* depthImageRenderContext;
			uint32_t currentSyncFrame = 0; // Current Synchronized Frame (Ordered).
			uint32_t currentSwapFrame = 0; // Current SwapChain Image Frame (Out of Order).

			void CreateImageSyncObjects() {
				size_t count = static_cast<size_t>(bufferingMode);
				imageAvailableSemaphores.resize(count);
				renderFinishedSemaphores.resize(count);
				inFlightFences.resize(count);

				VkSemaphoreCreateInfo semaphoreInfo{};
				semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

				VkFenceCreateInfo fenceInfo{};
				fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
				fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

				for (size_t i = 0; i < images.size(); i++) {
					if (vkCreateSemaphore(renderContext.vkdevice.GetLogicalDevice(), &semaphoreInfo, VK_NULL_HANDLE, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
						vkCreateSemaphore(renderContext.vkdevice.GetLogicalDevice(), &semaphoreInfo, VK_NULL_HANDLE, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
						vkCreateFence(renderContext.vkdevice.GetLogicalDevice(), &fenceInfo, VK_NULL_HANDLE, &inFlightFences[i]) != VK_SUCCESS)
						throw std::runtime_error("TinyVulkan: Failed to create synchronization objects for a frame!");
				}
			}

			VkResult RendererAcquireImage() {
				vkWaitForFences(renderContext.vkdevice.GetLogicalDevice(), 1, &inFlightFences[currentSyncFrame], VK_TRUE, UINT64_MAX);
				VkResult result = AcquireNextImage(imageAvailableSemaphores[currentSyncFrame], VK_NULL_HANDLE, currentSwapFrame);
				vkResetFences(renderContext.vkdevice.GetLogicalDevice(), 1, &inFlightFences[currentSyncFrame]);
				return result;
			}

			VkResult RendererExecuteEvents() {
				commandPools[currentSyncFrame]->ReturnAllBuffers(true);

				if (renderContext.graphicsPipeline.DepthTestingIsEnabled()) {
					TinyVkImage* depthImage = optionalDepthImages[currentSyncFrame];
					if (depthImage->width != imageExtent.width || depthImage->height != imageExtent.height) {
						depthImage->Disposable(false);
						depthImage->ReCreateImage(imageExtent.width, imageExtent.height, depthImage->isDepthImage, renderContext.graphicsPipeline.QueryDepthFormat(), TINYVK_DEPTHSTENCIL_ATTACHMENT_OPTIMAL, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_ASPECT_DEPTH_BIT);
					}
				}

				frameRenderSizes[currentSyncFrame] = imageExtent;
				commandPools[currentSyncFrame]->ReturnAllBuffers(true);
				onRenderEvents.invoke(*commandPools[currentSyncFrame]);
				return VK_SUCCESS;
			}

			VkResult RendererSubmitPresent() {
				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

				VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentSyncFrame] };
				VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
				submitInfo.waitSemaphoreCount = 1;
				submitInfo.pWaitSemaphores = waitSemaphores;
				submitInfo.pWaitDstStageMask = waitStages;
				
				std::vector<VkCommandBuffer> commandBuffers;
				auto buffers = commandPools[currentSyncFrame]->GetBuffers();
				std::for_each(buffers.begin(), buffers.end(),
					[&commandBuffers](std::pair<VkCommandBuffer, VkBool32> cmdBuffer){
						if (cmdBuffer.second) commandBuffers.push_back(cmdBuffer.first);
				});

				submitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
				submitInfo.pCommandBuffers = commandBuffers.data();

				VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentSyncFrame] };
				submitInfo.signalSemaphoreCount = 1;
				submitInfo.pSignalSemaphores = signalSemaphores;

				if (vkQueueSubmit(renderContext.graphicsPipeline.GetGraphicsQueue(), 1, &submitInfo, inFlightFences[currentSyncFrame]) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to submit draw command buffer!");

				VkPresentInfoKHR presentInfo{};
				presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
				presentInfo.waitSemaphoreCount = 1;
				presentInfo.pWaitSemaphores = signalSemaphores;

				VkSwapchainKHR swapChainList[]{ swapChain };
				presentInfo.swapchainCount = 1;
				presentInfo.pSwapchains = swapChainList;
				presentInfo.pImageIndices = &currentSwapFrame;

				currentSyncFrame = (currentSyncFrame + 1) % static_cast<size_t>(images.size());
				return vkQueuePresentKHR(renderContext.graphicsPipeline.GetPresentQueue(), &presentInfo);
			}

			void RenderSwapChain() {
				if (refreshable) { OnFrameBufferResizeCallbackNoLock(window.GetHandle(), window.GetWidth(), window.GetHeight()); return; }
				if (!presentable) return;

				VkResult result = VK_SUCCESS;
				result = RendererAcquireImage();
				if (result == VK_SUCCESS) {
					result = RendererExecuteEvents();
					if (result == VK_SUCCESS)
						result = RendererSubmitPresent();
				}

				if (result == VK_ERROR_OUT_OF_DATE_KHR) {
					commandPools[currentSyncFrame]->ReturnAllBuffers(true);
					presentable = false;
					currentSyncFrame = 0;
				} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
					throw std::runtime_error("TinyVulkan: Failed to acquire swap chain image or submit to draw queue!");
			}
		public:
			TinyVkRenderContext& renderContext;
			TinyVkWindow& window;

			inline static TinyVkInvokable<int, int> onResizeFrameBuffer;
			std::atomic_bool presentable, refreshable;

			TinyVkInvokable<TinyVkCommandPool&> onRenderEvents;

			TinyVkSwapChainRenderer operator=(const TinyVkSwapChainRenderer& swapRenderer) = delete;

			~TinyVkSwapChainRenderer() { this->Dispose(); }

			void Disposable(bool waitIdle) {
				if (waitIdle) renderContext.vkdevice.DeviceWaitIdle();

				if (renderContext.graphicsPipeline.DepthTestingIsEnabled()) {
					for(size_t i = 0; i < static_cast<size_t>(bufferingMode); i++) {
						optionalDepthImages[i]->Dispose();
						delete optionalDepthImages[i];
					}

					depthImagePool->Dispose();
					delete depthImagePool;
					delete depthImageRenderContext;
				}

				for (TinyVkCommandPool* cmdPool : commandPools) {
					cmdPool->Dispose();
					delete cmdPool;
				}

				for (size_t i = 0; i < inFlightFences.size(); i++) {
					vkDestroySemaphore(renderContext.vkdevice.GetLogicalDevice(), imageAvailableSemaphores[i], VK_NULL_HANDLE);
					vkDestroySemaphore(renderContext.vkdevice.GetLogicalDevice(), renderFinishedSemaphores[i], VK_NULL_HANDLE);
					vkDestroyFence(renderContext.vkdevice.GetLogicalDevice(), inFlightFences[i], VK_NULL_HANDLE);
				}

				for (auto imageView : imageViews)
					vkDestroyImageView(renderContext.vkdevice.GetLogicalDevice(), imageView, VK_NULL_HANDLE);

				vkDestroySwapchainKHR(renderContext.vkdevice.GetLogicalDevice(), swapChain, VK_NULL_HANDLE);
			}

			/// <summary>Creates a renderer specifically for performing render commands on a TinyVkSwapChain (VkSwapChain) to present to the window.</summary>
			TinyVkSwapChainRenderer(TinyVkRenderContext& renderContext, TinyVkWindow& window, const TinyVkBufferingMode bufferingMode, size_t cmdpoolbuffercount = TinyVkCommandPool::GetDefaultPoolSize(), TinyVkSurfaceSupporter presentDetails = TinyVkSurfaceSupporter(), VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
				: renderContext(renderContext), window(window), bufferingMode(bufferingMode), presentDetails(presentDetails), imageUsage(imageUsage), presentable(true) {
				onDispose.hook(TinyVkCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
				onResizeFrameBuffer.hook(TinyVkCallback<int, int>([this](int, int){ this->RenderSwapChain(); }));
				window.onResizeFrameBuffer.hook(TinyVkCallback<GLFWwindow*, int, int>([this](GLFWwindow* w, int x, int y) { this->OnFrameBufferResizeCallback(w, x, y); }));

				for(size_t i = 0; i < static_cast<size_t>(bufferingMode); i++) {
					commandPools.push_back(new TinyVkCommandPool(renderContext.vkdevice, cmdpoolbuffercount));
					frameRenderSizes.push_back({ imageExtent.width, imageExtent.height });
				}

				if (renderContext.graphicsPipeline.DepthTestingIsEnabled()) {
					depthImagePool = new TinyVkCommandPool(renderContext.vkdevice, static_cast<size_t>(bufferingMode));
					depthImageRenderContext = new TinyVkRenderContext(renderContext.vkdevice, *depthImagePool, renderContext.graphicsPipeline);
					
					for(size_t i = 0; i < static_cast<size_t>(bufferingMode); i++)
						optionalDepthImages.push_back(new TinyVkImage(*depthImageRenderContext, imageExtent.width, imageExtent.height, true, renderContext.graphicsPipeline.QueryDepthFormat(), TINYVK_DEPTHSTENCIL_ATTACHMENT_OPTIMAL, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_IMAGE_ASPECT_DEPTH_BIT));
				}

				CreateSwapChain();
				CreateImageSyncObjects();
			}

			/// <summary>Acquires the next image from the swap chain and returns out that image index.</summary>
			VkResult AcquireNextImage(VkSemaphore semaphore, VkFence fence, uint32_t& imageIndex) {
				return vkAcquireNextImageKHR(renderContext.vkdevice.GetLogicalDevice(), swapChain, UINT64_MAX, semaphore, fence, &imageIndex);
			}

			/// <summary>Notify the render engine that the window's frame buffer needs to be refreshed (without thread locking).</summary>
			void OnFrameBufferResizeCallbackNoLock(GLFWwindow* hwndWindow, int width, int height) {
				if (hwndWindow != window.GetHandle()) return;

				if (width > 0 && height > 0) {
					vkDeviceWaitIdle(renderContext.vkdevice.GetLogicalDevice());

					for (auto imageView : imageViews)
						vkDestroyImageView(renderContext.vkdevice.GetLogicalDevice(), imageView, VK_NULL_HANDLE);
					
					VkSwapchainKHR oldSwapChain = swapChain;
					CreateSwapChain(width, height);
					vkDestroySwapchainKHR(renderContext.vkdevice.GetLogicalDevice(), oldSwapChain, VK_NULL_HANDLE);

					presentable = true;
					refreshable = false;
					onResizeFrameBuffer.invoke(imageExtent.width, imageExtent.height);
				}
			}
			
			/// <summary>Notify the render engine that the window's frame buffer needs to be refreshed (with thread locking).</summary>
			void OnFrameBufferResizeCallback(GLFWwindow* hwndWindow, int width, int height) {
				timed_guard<true> swapChainLock(swapChainMutex);
				if (!swapChainLock.Acquired()) return;
				OnFrameBufferResizeCallbackNoLock(hwndWindow, width, height);
			}

			/// <summary>Returns the current resource synchronized frame index.</summary>
			size_t GetSyncronizedFrameIndex() { return currentSyncFrame; }

			/// <summary>Begins recording render commands to the provided command buffer.</summary>
			void BeginRecordCmdBuffer(VkCommandBuffer commandBuffer, const VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f }, const VkClearValue depthStencil = { .depthStencil = { 1.0f, 0 } }) {
				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
				beginInfo.pInheritanceInfo = VK_NULL_HANDLE; // Optional

				if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to record [begin] to command buffer!");

				const VkImageMemoryBarrier swapchain_memory_barrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.image = images[currentSwapFrame],
					.subresourceRange = {
					  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					  .baseMipLevel = 0,
					  .levelCount = 1,
					  .baseArrayLayer = 0,
					  .layerCount = 1,
					},
				};

				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &swapchain_memory_barrier);

				VkRenderingAttachmentInfoKHR colorAttachmentInfo{};
				colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
				colorAttachmentInfo.imageView = imageViews[currentSwapFrame];
				colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				colorAttachmentInfo.clearValue = clearColor;

				VkRenderingInfoKHR dynamicRenderInfo{};
				dynamicRenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;

				VkRect2D renderAreaKHR{};
				renderAreaKHR.extent = frameRenderSizes[currentSyncFrame];
				renderAreaKHR.offset = { 0,0 };
				dynamicRenderInfo.renderArea = renderAreaKHR;
				dynamicRenderInfo.layerCount = 1;
				dynamicRenderInfo.colorAttachmentCount = 1;
				dynamicRenderInfo.pColorAttachments = &colorAttachmentInfo;
				
				VkRenderingAttachmentInfoKHR depthStencilAttachmentInfo{};
				if (renderContext.graphicsPipeline.DepthTestingIsEnabled()) {
					const VkImageMemoryBarrier depth_memory_barrier{
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
						.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
						.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
						.image = optionalDepthImages[currentSyncFrame]->image,
						.subresourceRange = {
						  .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
						  .baseMipLevel = 0,
						  .levelCount = 1,
						  .baseArrayLayer = 0,
						  .layerCount = 1,
						},
					};

					vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &depth_memory_barrier);

					depthStencilAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
					depthStencilAttachmentInfo.imageView = optionalDepthImages[currentSyncFrame]->imageView;
					depthStencilAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					depthStencilAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
					depthStencilAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
					depthStencilAttachmentInfo.clearValue = depthStencil;
					dynamicRenderInfo.pDepthAttachment = &depthStencilAttachmentInfo;
				}

				VkViewport dynamicViewportKHR{};
				dynamicViewportKHR.x = 0;
				dynamicViewportKHR.y = 0;
				dynamicViewportKHR.width = static_cast<float>(frameRenderSizes[currentSyncFrame].width);
				dynamicViewportKHR.height = static_cast<float>(frameRenderSizes[currentSyncFrame].height);
				dynamicViewportKHR.minDepth = 0.0f;
				dynamicViewportKHR.maxDepth = 1.0f;

				vkCmdSetViewport(commandBuffer, 0, 1, &dynamicViewportKHR);
				vkCmdSetScissor(commandBuffer, 0, 1, &renderAreaKHR);

				if (vkCmdBeginRenderingEKHR(renderContext.vkdevice.GetInstance(), commandBuffer, &dynamicRenderInfo) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to record [begin] to rendering!");
				
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderContext.graphicsPipeline.GetGraphicsPipeline());
			}

			/// <summary>Ends recording render commands to the provided command buffer.</summary>
			void EndRecordCmdBuffer(VkCommandBuffer commandBuffer, const VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f }, const VkClearValue depthStencil = { .depthStencil = { 1.0f, 0 } }) {
				if (vkCmdEndRenderingEKHR(renderContext.vkdevice.GetInstance(), commandBuffer) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to record [end] to rendering!");

				const VkImageMemoryBarrier swapchain_memory_barrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
					.image = images[currentSwapFrame],
					.subresourceRange = {
					  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					  .baseMipLevel = 0,
					  .levelCount = 1,
					  .baseArrayLayer = 0,
					  .layerCount = 1,
					}
				};

				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &swapchain_memory_barrier);

				if (renderContext.graphicsPipeline.DepthTestingIsEnabled()) {
					const VkImageMemoryBarrier depth_memory_barrier{
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
						.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
						.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
						.image = optionalDepthImages[currentSyncFrame]->image,
						.subresourceRange = {
						  .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
						  .baseMipLevel = 0,
						  .levelCount = 1,
						  .baseArrayLayer = 0,
						  .layerCount = 1,
						},
					};

					vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &depth_memory_barrier);
				}

				if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to record [end] to command buffer!");
			}

			/// <summary>Records Push Descriptors to the command buffer.</summary>
			VkResult PushDescriptorSet(VkCommandBuffer cmdBuffer, std::vector<VkWriteDescriptorSet> writeDescriptorSets) {
				return vkCmdPushDescriptorSetEKHR(renderContext.vkdevice.GetInstance(), cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderContext.graphicsPipeline.GetPipelineLayout(),
					0, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data());
			}

			/// <summary>Records Push Constants to the command buffer.</summary>
			void PushConstants(VkCommandBuffer cmdBuffer, VkShaderStageFlagBits shaderFlags, uint32_t byteSize, const void* pValues) {
				vkCmdPushConstants(cmdBuffer, renderContext.graphicsPipeline.GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, byteSize, pValues);
			}

			/// <summary>Executes the registered onRenderEvents and presents them to the SwapChain(Window).</summary>
			void RenderExecute() {
				timed_guard swapChainLock(swapChainMutex);
				if (!swapChainLock.Acquired()) return;
				RenderSwapChain();
			}

			//VkPresentModeKHR newPresentMode;
			void PushPresentMode(VkPresentModeKHR presentMode) {
				if (presentMode != presentDetails.idealPresentMode) {
					presentDetails = { presentDetails.dataFormat, presentDetails.colorSpace, presentMode };
					refreshable = true;
				}
			}
		};
	}
#endif