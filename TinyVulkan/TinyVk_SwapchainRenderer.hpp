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
		
		/// @brief Onscreen Rendering (Render/Present-To-Screen Model): Render to SwapChain.
		class TinyVkSwapchainRenderer : public TinyVkDisposable, public TinyVkGraphicsRenderer {
		private:
			std::timed_mutex swapChainMutex;
			TinyVkSurfaceSupporter presentDetails;
			VkSwapchainKHR swapChain = VK_NULL_HANDLE;
			VkFormat imageFormat;
			VkExtent2D imageExtent;
			VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			TinyVkBufferingMode bufferingMode;
			
			std::vector<TinyVkImage*> imageSources;
			std::vector<TinyVkImage*> imageDepthSources;
			std::vector<VkSemaphore> imageAvailable;
			std::vector<VkSemaphore> imageFinished;
			std::vector<VkFence> imageInFlight;
			std::vector<TinyVkCommandPool*> imageCmdPools;

			uint32_t currentSyncFrame = 0; // Current Synchronized Frame (Ordered).
			uint32_t currentSwapFrame = 0; // Current SwapChain Image Frame (Out of Order).
			std::atomic_bool presentable, refreshable;

			#pragma region SWAPCHAIN_CREATION_AND_RECREATION
			
			/// @brief Create the Vulkan surface swap-chain images and imageviews.
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
				if (!indices.HasGraphicsFamily() || !indices.HasPresentFamily())
					throw TinyVkRuntimeError("TinyVulkan: Could not locate graphics and present queue families for TinyVkSwapchainRenderer!");
								
				uint32_t queueFamilyIndices[] = { indices.graphicsFamily, indices.presentFamily };

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
					throw TinyVkRuntimeError("TinyVulkan: Failed to create swap chain!");

				vkGetSwapchainImagesKHR(renderContext.vkdevice.GetLogicalDevice(), swapChain, &imageCount, VK_NULL_HANDLE);
				
				std::vector<VkImage> newSwapImages;
				newSwapImages.resize(imageCount);
				vkGetSwapchainImagesKHR(renderContext.vkdevice.GetLogicalDevice(), swapChain, &imageCount, newSwapImages.data());

				imageSources.resize(imageCount);
				for(uint32_t i = 0; i < imageCount; i++)
					imageSources[i] = new TinyVkImage(renderContext, TinyVkImageType::TINYVK_IMAGE_TYPE_SWAPCHAIN, extent.width, extent.height, newSwapImages[i], VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

				imageFormat = surfaceFormat.format;
				imageExtent = extent;
			}

			/// @brief Create the image views for rendering to images (including those in the swap-chain).
			void CreateSwapChainImageViews(VkImageViewCreateInfo* createInfoEx = VK_NULL_HANDLE) {
				for (size_t i = 0; i < imageSources.size(); i++) {
					VkImageViewCreateInfo createInfo{};

					if (createInfoEx == VK_NULL_HANDLE) {
						createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
						createInfo.image = imageSources[i]->image;
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

					if (vkCreateImageView(renderContext.vkdevice.GetLogicalDevice(), &createInfo, VK_NULL_HANDLE, &imageSources[i]->imageView) != VK_SUCCESS)
						throw TinyVkRuntimeError("TinyVulkan: Failed to create swap chain image views!");
				}
			}

			/// @brief Create the Vulkan surface swapchain.
			void CreateSwapChain(uint32_t width = 0, uint32_t height = 0) {
				CreateSwapChainImages(width, height);
				CreateSwapChainImageViews();
			}

			/// @brief Creates the synchronization semaphores/fences for swapchain multi-frame buffering.
			void CreateImageSyncObjects() {
				size_t count = static_cast<size_t>(bufferingMode);
				imageAvailable.resize(count);
				imageFinished.resize(count);
				imageInFlight.resize(count);

				VkSemaphoreCreateInfo semaphoreInfo{};
				semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

				VkFenceCreateInfo fenceInfo{};
				fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
				fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

				for (size_t i = 0; i < imageSources.size(); i++) {
					if (vkCreateSemaphore(renderContext.vkdevice.GetLogicalDevice(), &semaphoreInfo, VK_NULL_HANDLE, &imageAvailable[i]) != VK_SUCCESS ||
						vkCreateSemaphore(renderContext.vkdevice.GetLogicalDevice(), &semaphoreInfo, VK_NULL_HANDLE, &imageFinished[i]) != VK_SUCCESS ||
						vkCreateFence(renderContext.vkdevice.GetLogicalDevice(), &fenceInfo, VK_NULL_HANDLE, &imageInFlight[i]) != VK_SUCCESS)
						throw TinyVkRuntimeError("TinyVulkan: Failed to create synchronization objects for a frame!");
					
					imageSources[i]->imageAvailable = imageAvailable[i];
					imageSources[i]->imageFinished = imageFinished[i];
					imageSources[i]->imageWaitable = imageInFlight[i];
				}
			}

			#pragma endregion
			#pragma region SWAPCHAIN_PROPERTY_QUERYING
			
			/// @brief Checks the VkPhysicalDevice for swap-chain availability.
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

			/// @brief Gets the swap-chain surface format.
			VkSurfaceFormatKHR QuerySwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
				for (const auto& availableFormat : availableFormats)
					if (availableFormat.format == presentDetails.dataFormat && availableFormat.colorSpace == presentDetails.colorSpace)
						return availableFormat;

				return availableFormats[0];
			}

			/// @brief Select the swap-chains active present mode.
			VkPresentModeKHR QuerySwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
				for (const auto& availablePresentMode : availablePresentModes)
					if (availablePresentMode == presentDetails.idealPresentMode)
						return availablePresentMode;

				return VK_PRESENT_MODE_FIFO_KHR;
			}

			/// @brief Select swap-chain extent (swap-chain surface resolution).
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

			#pragma endregion
			#pragma region RENDER_SUBMISSION_AND_EXECUTION
			
			/// @brief Acquires the next image from the swap chain and returns out that image index.
			VkResult QueryNextImage() {
				vkWaitForFences(renderContext.vkdevice.GetLogicalDevice(), 1, &imageInFlight[currentSyncFrame], VK_TRUE, UINT64_MAX);
				vkResetFences(renderContext.vkdevice.GetLogicalDevice(), 1, &imageInFlight[currentSyncFrame]);
				return vkAcquireNextImageKHR(renderContext.vkdevice.GetLogicalDevice(), swapChain, UINT64_MAX, imageAvailable[currentSyncFrame], VK_NULL_HANDLE, &currentSwapFrame);
			}

			VkResult RenderPresent() {
				VkSemaphore signalSemaphores[] = { renderTarget->imageFinished };
				
				VkPresentInfoKHR presentInfo{};
				presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
				presentInfo.waitSemaphoreCount = 1;
				presentInfo.pWaitSemaphores = signalSemaphores;

				VkSwapchainKHR swapChainList[]{ swapChain };
				presentInfo.swapchainCount = 1;
				presentInfo.pSwapchains = swapChainList;
				presentInfo.pImageIndices = &currentSwapFrame;

				currentSyncFrame = (currentSyncFrame + 1) % static_cast<size_t>(imageSources.size());
				return vkQueuePresentKHR(renderContext.graphicsPipeline.GetPresentQueue(), &presentInfo);
			}

			VkResult RenderSwapChain() {
				if (refreshable) { OnFrameBufferResizeCallbackNoLock(window.hwndWindow, window.hwndWidth, window.hwndHeight); return VK_SUBOPTIMAL_KHR; }
				if (!presentable) return VK_ERROR_OUT_OF_DATE_KHR;
				
				VkResult result = QueryNextImage();
				TinyVkImage* swapDepthImage = (renderContext.graphicsPipeline.DepthTestingIsEnabled())? imageDepthSources[currentSyncFrame]: VK_NULL_HANDLE;
				
				imageSources[currentSwapFrame]->imageAvailable = imageAvailable[currentSyncFrame];
				imageSources[currentSwapFrame]->imageFinished = imageFinished[currentSyncFrame];
				imageSources[currentSwapFrame]->imageWaitable = imageInFlight[currentSyncFrame];
				
				this->SetRenderTarget(imageCmdPools[currentSyncFrame], imageSources[currentSwapFrame], swapDepthImage, false);

				if (result == VK_SUCCESS) {
					result = this->TinyVkGraphicsRenderer::RenderExecute(false);
					if (result == VK_SUCCESS)
						result = this->RenderPresent();
				}
				
				if (result == VK_ERROR_OUT_OF_DATE_KHR) {
					imageCmdPools[currentSyncFrame]->ReturnAllBuffers();
					presentable = false;
					currentSyncFrame = 0;
				} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
					throw TinyVkRuntimeError("TinyVulkan: Failed to acquire swap chain image or submit to draw queue!");
				
				return result;
			}

			#pragma endregion
		public:
			TinyVkWindow& window;

			inline static TinyVkInvokable<int, int> onResizeFrameBuffer;

			TinyVkSwapchainRenderer operator=(const TinyVkSwapchainRenderer& swapRenderer) = delete;

			~TinyVkSwapchainRenderer() { this->Dispose(); }

			void Disposable(bool waitIdle) {
				if (waitIdle) renderContext.vkdevice.DeviceWaitIdle();

				if (renderContext.graphicsPipeline.DepthTestingIsEnabled()) {
					for(size_t i = 0; i < static_cast<size_t>(bufferingMode); i++) {
						imageDepthSources[i]->Dispose();
						delete imageDepthSources[i];
					}
				}

				for (TinyVkCommandPool* cmdPool : imageCmdPools) {
					cmdPool->Dispose();
					delete cmdPool;
				}

				for (size_t i = 0; i < imageInFlight.size(); i++) {
					vkDestroySemaphore(renderContext.vkdevice.GetLogicalDevice(), imageAvailable[i], VK_NULL_HANDLE);
					vkDestroySemaphore(renderContext.vkdevice.GetLogicalDevice(), imageFinished[i], VK_NULL_HANDLE);
					vkDestroyFence(renderContext.vkdevice.GetLogicalDevice(), imageInFlight[i], VK_NULL_HANDLE);
				}

				for(auto image : imageSources) {
					vkDestroyImageView(renderContext.vkdevice.GetLogicalDevice(), image->imageView, VK_NULL_HANDLE);
					delete image;
				}

				vkDestroySwapchainKHR(renderContext.vkdevice.GetLogicalDevice(), swapChain, VK_NULL_HANDLE);
			}

			/// @brief Creates a renderer specifically for performing render commands on a TinyVkSwapChain (VkSwapChain) to present to the window.
			TinyVkSwapchainRenderer(TinyVkRenderContext& renderContext, TinyVkWindow& window, const TinyVkBufferingMode bufferingMode, size_t cmdpoolbuffercount = TinyVkCommandPool::GetDefaultPoolSize(), TinyVkSurfaceSupporter presentDetails = TinyVkSurfaceSupporter(), VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
				: window(window), bufferingMode(bufferingMode), presentDetails(presentDetails), imageUsage(imageUsage), presentable(true), TinyVkGraphicsRenderer(renderContext, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE) {
				onDispose.hook(TinyVkCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
				onResizeFrameBuffer.hook(TinyVkCallback<int, int>([this](int, int){ this->RenderSwapChain(); }));
				window.onResizeFrameBuffer.hook(TinyVkCallback<GLFWwindow*, int, int>([this](GLFWwindow* w, int x, int y) { this->OnFrameBufferResizeCallback(w, x, y); }));
				imageExtent = (VkExtent2D) { static_cast<uint32_t>(window.hwndWidth), static_cast<uint32_t>(window.hwndHeight) };

				for(size_t i = 0; i < static_cast<size_t>(bufferingMode); i++)
					imageCmdPools.push_back(new TinyVkCommandPool(renderContext.vkdevice, false, cmdpoolbuffercount));
				
				if (renderContext.graphicsPipeline.DepthTestingIsEnabled())
					for(size_t i = 0; i < static_cast<size_t>(bufferingMode); i++)
						imageDepthSources.push_back(new TinyVkImage(renderContext, TinyVkImageType::TINYVK_IMAGE_TYPE_DEPTHSTENCIL, imageExtent.width, imageExtent.height, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, renderContext.graphicsPipeline.QueryDepthFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT));
				
				CreateSwapChain();
				CreateImageSyncObjects();
			}

			#pragma region SWAPCHAIN_RESIZE_CALLBACKS
			
			/// @brief Notify the render engine that the window's frame buffer needs to be refreshed (without thread locking).
			void OnFrameBufferResizeCallbackNoLock(GLFWwindow* hwndWindow, int width, int height) {
				if (hwndWindow != window.hwndWindow) return;

				if (width > 0 && height > 0) {
					renderContext.vkdevice.DeviceWaitIdle();

					for(auto swapImage : imageSources) {
						vkDestroyImageView(renderContext.vkdevice.GetLogicalDevice(), swapImage->imageView, VK_NULL_HANDLE);
						delete swapImage;
					}

					VkSwapchainKHR oldSwapChain = swapChain;
					CreateSwapChain(width, height);
					vkDestroySwapchainKHR(renderContext.vkdevice.GetLogicalDevice(), oldSwapChain, VK_NULL_HANDLE);

					presentable = true;
					refreshable = false;
					imageExtent = (VkExtent2D) { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
					onResizeFrameBuffer.invoke(imageExtent.width, imageExtent.height);
				}
			}
			
			/// @brief Notify the render engine that the window's frame buffer needs to be refreshed (with thread locking).
			void OnFrameBufferResizeCallback(GLFWwindow* hwndWindow, int width, int height) {
				timed_guard<true> swapChainLock(swapChainMutex);
				if (!swapChainLock.Acquired()) return;
				OnFrameBufferResizeCallbackNoLock(hwndWindow, width, height);
			}

			#pragma endregion
			#pragma region REFERENCE_GETTERS
			
			/// @brief Returns the current resource synchronized frame index.
			uint32_t GetSyncronizedFrameIndex() { return currentSyncFrame; }
			
			/// @brief Returns reference to presentable atomic_bool (whether swapchain is presentable or not).
			std::atomic_bool& GetPresentableBool() { return presentable; }
			
			/// @brief Returns reference to presentable atomic_bool (whether swapchain is NOT presentable and needs a refresh).
			std::atomic_bool& GetRefreshableBool() { return refreshable; }

			#pragma endregion
			#pragma region RENDER_SUBMISSION_AND_EXECUTION
			
			/// @brief Returns the current resource synchronized frame index.
			void PushPresentMode(VkPresentModeKHR presentMode) {
				if (presentMode != presentDetails.idealPresentMode) {
					presentDetails = { presentDetails.dataFormat, presentDetails.colorSpace, presentMode };
					refreshable = true;
				}
			}

			/// @brief Executes the registered onRenderEvents and presents them to the SwapChain(Window).
			VkResult RenderExecute(bool waitFences = true) override {
				timed_guard swapChainLock(swapChainMutex);
				if (!swapChainLock.Acquired()) return VK_ERROR_OUT_OF_DATE_KHR;
				return RenderSwapChain();
			}

			#pragma endregion
		};
	}
#endif
