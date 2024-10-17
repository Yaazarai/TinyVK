#ifndef TINYVK_TINYVKIMAGE
#define TINYVK_TINYVKIMAGE
	#include "./TinyVulkan.hpp"

	namespace TINYVULKAN_NAMESPACE {
		/*
			ABOUT BUFFERS & IMAGES:
				When Creating Buffers:
					Buffers must be initialized with a VkDeviceSize, which is the size of the data in BYTES, not the
					size of the data container (number of items). This same principle applies to stagging buffer data.

				There are 3 types of GPU dedicated memory buffers:
					Vertex:		Allows you to send mesh triangle data to the GPU.
					Index:		Allws you to send mapped indices for vertex buffers to the GPU.
					Uniform:	Allows you to send data to shaders using uniforms.
						* Push Constants are an alternative that do not require buffers, simply use: vkCmdPushConstants(...).

				The last buffer type is a CPU memory buffer for transfering data from the CPU to the GPU:
					Staging:	Staging CPU data for transfer to the GPU.

				Render images are for rendering sprites or textures on the GPU (similar to the swap chain, but handled manually).
					The default image layout is: VK_IMAGE_LAYOUT_UNDEFINED
					To render to shaders you must change/transition the layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
					Once the layout is set for transfering you can write data to the image from CPU memory to GPU memory.
					Finally for use in shaders you need to change the layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
		*/

		/// @brief Specifies the [layout] in GPU memory of image and how it can be used..
		enum TinyVkImageLayout {
			TINYVK_TRANSFER_SRC = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			TINYVK_TRANSFER_DST = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			TINYVK_SHADER_READONLY = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			TINYVK_DEPTHSTENCIL_ATTACHMENT = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			TINYVK_UNDEFINED = VK_IMAGE_LAYOUT_UNDEFINED,
			TINYVK_COLOR_ATTACHMENT = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			TINYVK_GENERAL = VK_IMAGE_LAYOUT_GENERAL,
			TINYVK_PRESENT_SRC = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		};
		/// @brief Specifies the [type] of image that this TinyVkImage represents.
		enum class TinyVkImageType {
			TINYVK_IMAGE_TYPE_SWAPCHAIN,       /// For writing or presenting with Swapchain (screen).
			TINYVK_IMAGE_TYPE_COLORATTACHMENT, /// For reading OR writing from/to VkImage via Fragment shaders.
			TINYVK_IMAGE_TYPE_STORAGE,         /// For reading/writing directly from/to VkImage via Compute shaders.
			TINYVK_IMAGE_TYPE_DEPTHSTENCIL     /// For reading/writing depth/stencil shader information.
		};

		/// @brief GPU device image for sending images to the render (GPU) device.
		class TinyVkImage : public TinyVkDisposable {
		private:
			void CreateImageView() {
				VkImageViewCreateInfo createInfo{};
				createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				createInfo.image = image;
				createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				createInfo.format = format;
				createInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY };
				createInfo.subresourceRange = { .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1, .aspectMask = aspectFlags, };

				if (vkCreateImageView(renderContext.vkdevice.GetLogicalDevice(), &createInfo, VK_NULL_HANDLE, &imageView) != VK_SUCCESS)
					throw TinyVkRuntimeError("TinyVulkan: Failed to create TinyVkImage view!");
			}

			void CreateTextureSampler() {
				VkSamplerCreateInfo samplerInfo {};
				samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
				samplerInfo.magFilter = VK_FILTER_LINEAR;
				samplerInfo.minFilter = VK_FILTER_LINEAR;
				samplerInfo.addressModeU = addressingMode;
				samplerInfo.addressModeV = addressingMode;
				samplerInfo.addressModeW = addressingMode;
				samplerInfo.anisotropyEnable = VK_FALSE;
				
				VkPhysicalDeviceProperties properties {};
				vkGetPhysicalDeviceProperties(renderContext.vkdevice.GetPhysicalDevice(), &properties);
				samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

				samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
				samplerInfo.unnormalizedCoordinates = VK_FALSE;
				samplerInfo.compareEnable = VK_FALSE;
				samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				samplerInfo.mipLodBias = 0.0f;
				samplerInfo.minLod = 0.0f;
				samplerInfo.maxLod = 0.0f;

				if (vkCreateSampler(renderContext.vkdevice.GetLogicalDevice(), &samplerInfo, VK_NULL_HANDLE, &imageSampler) != VK_SUCCESS)
					throw TinyVkRuntimeError("TinyVulkan: Failed to create image texture sampler!");
			}

			void CreateImageSyncObjects() {
				VkSemaphoreCreateInfo semaphoreInfo{};
				semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
				semaphoreInfo.flags = VK_SEMAPHORE_TYPE_BINARY;

				VkFenceCreateInfo fenceInfo{};
				fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
				fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

				if (vkCreateSemaphore(renderContext.vkdevice.GetLogicalDevice(), &semaphoreInfo, VK_NULL_HANDLE, &imageAvailable) != VK_SUCCESS ||
					vkCreateSemaphore(renderContext.vkdevice.GetLogicalDevice(), &semaphoreInfo, VK_NULL_HANDLE, &imageFinished) != VK_SUCCESS ||
					vkCreateFence(renderContext.vkdevice.GetLogicalDevice(), &fenceInfo, VK_NULL_HANDLE, &imageWaitable) != VK_SUCCESS)
					throw TinyVkRuntimeError("TinyVulkan: Failed to create synchronization objects for a image renderer!");
			}
		public:
			std::timed_mutex image_lock;

			VmaAllocation memory = VK_NULL_HANDLE;
			VkImage image = VK_NULL_HANDLE;
			VkImageView imageView = VK_NULL_HANDLE;
			VkSampler imageSampler = VK_NULL_HANDLE;
			TinyVkImageLayout imageLayout;
			VkImageAspectFlags aspectFlags;
			VkSamplerAddressMode addressingMode;

			VkSemaphore imageAvailable;
			VkSemaphore imageFinished;
			VkFence imageWaitable;

			VkDeviceSize width, height;
			VkFormat format;

			TinyVkRenderContext& renderContext;
			const TinyVkImageType imageType;

			/// @brief Deleted copy constructor (dynamic objects are not copyable).
			TinyVkImage operator=(const TinyVkImage& image) = delete;
			
			~TinyVkImage() { this->Dispose(); }

			void Disposable(bool waitIdle) {
				if (waitIdle) renderContext.vkdevice.DeviceWaitIdle();
				
				if (imageType != TinyVkImageType::TINYVK_IMAGE_TYPE_SWAPCHAIN) {
					vkDestroySampler(renderContext.vkdevice.GetLogicalDevice(), imageSampler, VK_NULL_HANDLE);
					vkDestroyImageView(renderContext.vkdevice.GetLogicalDevice(), imageView, VK_NULL_HANDLE);
					vmaDestroyImage(renderContext.vkdevice.GetAllocator(), image, memory);
					vkDestroySemaphore(renderContext.vkdevice.GetLogicalDevice(), imageAvailable, VK_NULL_HANDLE);
					vkDestroySemaphore(renderContext.vkdevice.GetLogicalDevice(), imageFinished, VK_NULL_HANDLE);
					vkDestroyFence(renderContext.vkdevice.GetLogicalDevice(), imageWaitable, VK_NULL_HANDLE);
				}
			}

			/// @brief Creates a VkImage for rendering or loading image files (stagedata) into.
			TinyVkImage(TinyVkRenderContext& renderContext, TinyVkImageType type, VkDeviceSize width, VkDeviceSize height, VkImage imageSource = VK_NULL_HANDLE, VkImageView imageViewSource = VK_NULL_HANDLE, VkSampler imageSampler = VK_NULL_HANDLE, VkSemaphore imageAvailable = VK_NULL_HANDLE, VkSemaphore imageFinished = VK_NULL_HANDLE, VkFence imageWaitable = VK_NULL_HANDLE, VkFormat format = VK_FORMAT_B8G8R8A8_UNORM, VkSamplerAddressMode addressingMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
			: renderContext(renderContext), imageType(type), width(width), height(height), image(imageSource), imageView(imageViewSource), imageSampler(imageSampler), imageAvailable(imageAvailable), imageFinished(imageFinished), imageWaitable(imageWaitable), format(format), imageLayout(TinyVkImageLayout::TINYVK_UNDEFINED), addressingMode(addressingMode), aspectFlags(aspectFlags) {
				onDispose.hook(TinyVkCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
				
				if (type == TinyVkImageType::TINYVK_IMAGE_TYPE_SWAPCHAIN) {
					if (imageSource == VK_NULL_HANDLE)
						throw TinyVkRuntimeError("TinyVulkan: passed SwapChain image is: VK_NULL_HANDLE");
					imageLayout = TinyVkImageLayout::TINYVK_UNDEFINED;
					aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
				} else {
					ReCreateImage(type, width, height, format, addressingMode);
				}
			}

			#pragma region IMAGE_INITIATION_AND_SYNCHRONIZATION

			/// @brief Recreates this TinyVkImage using a new layout/format (don't forget to call image.Disposable(bool waitIdle) to dispose of the previous image first.
			void ReCreateImage(TinyVkImageType type, VkDeviceSize width, VkDeviceSize height, VkFormat format = VK_FORMAT_R16G16B16A16_UNORM, VkSamplerAddressMode addressingMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE) {
				if (type == TinyVkImageType::TINYVK_IMAGE_TYPE_SWAPCHAIN)
					throw TinyVkRuntimeError("TinyVulkan: Tried to manually re-create swapchain allocated image!");

				VkImageCreateInfo imgCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.extent.width = static_cast<uint32_t>(width), .extent.height = static_cast<uint32_t>(height),
					.extent.depth = 1, .mipLevels = 1, .arrayLayers = 1,
					.format = format, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .imageType = VK_IMAGE_TYPE_2D,
					.tiling = VK_IMAGE_TILING_OPTIMAL, .samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
				};

				this->width = width;
				this->height = height;
				imageLayout = TinyVkImageLayout::TINYVK_UNDEFINED;

				TinyVkImageLayout newLayout;
				switch(imageType) {
					case TinyVkImageType::TINYVK_IMAGE_TYPE_DEPTHSTENCIL:
						newLayout = TinyVkImageLayout::TINYVK_DEPTHSTENCIL_ATTACHMENT;
						imgCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
						aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
					break;
					case TinyVkImageType::TINYVK_IMAGE_TYPE_STORAGE:
						newLayout = TinyVkImageLayout::TINYVK_GENERAL;
						imgCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
						aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
					break;
					case TinyVkImageType::TINYVK_IMAGE_TYPE_COLORATTACHMENT:
						newLayout = TinyVkImageLayout::TINYVK_COLOR_ATTACHMENT;
						imgCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
						aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
					break;
				}

				VmaAllocationCreateInfo allocCreateInfo {};
				allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
				allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
				allocCreateInfo.priority = 1.0f;
				
					if (vmaCreateImage(renderContext.vkdevice.GetAllocator(), &imgCreateInfo, &allocCreateInfo, &image, &memory, VK_NULL_HANDLE) != VK_SUCCESS)
						throw TinyVkRuntimeError("TinyVulkan: Could not allocate GPU image data for TinyVkImage!");
				
				CreateTextureSampler();
				CreateImageView();
				CreateImageSyncObjects();
				
				if (newLayout != TinyVkImageLayout::TINYVK_UNDEFINED)
					TransitionLayoutCmd(newLayout);
			}
			
			/// @brief Get pipeline stages relative to the current image layout and command buffer recording stage.
			void GetPipelineBarrierStages(TinyVkImageLayout layout, TinyVkCmdBufferSubmitStage cmdBufferStage, VkPipelineStageFlags& srcStage, VkPipelineStageFlags& dstStage, VkAccessFlags& srcAccessMask, VkAccessFlags& dstAccessMask) {
				if (cmdBufferStage == TinyVkCmdBufferSubmitStage::TINYVK_BEGIN) {
					switch (layout) {
						case TinyVkImageLayout::TINYVK_COLOR_ATTACHMENT:
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
						break;
						case TinyVkImageLayout::TINYVK_PRESENT_SRC:
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
						break;
						case TinyVkImageLayout::TINYVK_TRANSFER_SRC:
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						break;
						case TinyVkImageLayout::TINYVK_TRANSFER_DST:
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						break;
						case TinyVkImageLayout::TINYVK_SHADER_READONLY:
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
						break;
						case TinyVkImageLayout::TINYVK_DEPTHSTENCIL_ATTACHMENT:
							srcStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
							dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
							srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
							dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
						break;
						case TinyVkImageLayout::TINYVK_GENERAL:
							srcStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
							dstStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
							srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
							dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
						break;
						case TinyVkImageLayout::TINYVK_UNDEFINED:
						default:
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;;
						break;
					}
				}
				
				if (cmdBufferStage == TinyVkCmdBufferSubmitStage::TINYVK_END) {
					switch (layout) {
						case TinyVkImageLayout::TINYVK_COLOR_ATTACHMENT:
							srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
							dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
							srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
							dstAccessMask = VK_ACCESS_NONE;
						break;
						case TinyVkImageLayout::TINYVK_PRESENT_SRC:
							srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
							dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
							srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
							dstAccessMask = VK_ACCESS_NONE;
						break;
						case TinyVkImageLayout::TINYVK_TRANSFER_SRC:
							srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
							dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
							srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
							dstAccessMask = VK_ACCESS_NONE;
						break;
						case TinyVkImageLayout::TINYVK_TRANSFER_DST:
							srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
							dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
							srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
							dstAccessMask = VK_ACCESS_NONE;
						break;
						case TinyVkImageLayout::TINYVK_SHADER_READONLY:
							srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
							dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
							srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
							dstAccessMask = VK_ACCESS_NONE;
						break;
						case TinyVkImageLayout::TINYVK_DEPTHSTENCIL_ATTACHMENT:
							srcStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
							dstStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
							srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
							dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
						break;
						case TinyVkImageLayout::TINYVK_GENERAL:
							srcStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
							dstStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
							srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
							dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
						break;
						case TinyVkImageLayout::TINYVK_UNDEFINED:
						default:
							srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
							dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
							srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;;
							dstAccessMask = VK_ACCESS_NONE;
						break;
					}
				}

				if (cmdBufferStage == TinyVkCmdBufferSubmitStage::TINYVK_BEGIN_TO_END) {
					srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
					srcAccessMask = VK_ACCESS_NONE;
					dstAccessMask = VK_ACCESS_NONE;
				}
			}
					
			/// @brief Get the pipeline barrier info for resource synchronization in image pipeline barrier pNext chain.
			VkImageMemoryBarrier GetPipelineBarrier(TinyVkImageLayout newLayout, TinyVkCmdBufferSubmitStage cmdBufferStage, VkPipelineStageFlags& srcStage, VkPipelineStageFlags& dstStage) {
				VkImageMemoryBarrier pipelineBarrier = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.oldLayout = (VkImageLayout) imageLayout, .newLayout = (VkImageLayout) newLayout,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1, },
					.image = image,
				};

				if (imageLayout == TinyVkImageLayout::TINYVK_DEPTHSTENCIL_ATTACHMENT || newLayout == TinyVkImageLayout::TINYVK_DEPTHSTENCIL_ATTACHMENT) {
					pipelineBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
					if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT)
						pipelineBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
				}

				VkAccessFlags srcAccessMask, dstAccessMask;
				GetPipelineBarrierStages(newLayout, cmdBufferStage, srcStage, dstStage, srcAccessMask, dstAccessMask);
				pipelineBarrier.srcAccessMask = srcAccessMask;
				pipelineBarrier.dstAccessMask = dstAccessMask;
				return pipelineBarrier;
			}
			
			#pragma endregion
			#pragma region TRANSFER_TRANSITION_COMMANDS
			
			/// @brief Begins a transfer command and returns the command buffer index pair used for the command allocated from a TinyVkCommandPool.
			std::pair<VkCommandBuffer, int32_t> BeginTransferCmd() {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = renderContext.commandPool.LeaseBuffer(true);

				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
				vkBeginCommandBuffer(bufferIndexPair.first, &beginInfo);
				return bufferIndexPair;
			}

			/// @brief Ends a transfer command and gives the leased/rented command buffer pair back to the TinyVkCommandPool.
			void EndTransferCmd(std::pair<VkCommandBuffer, int32_t> bufferIndexPair) {
				vkEndCommandBuffer(bufferIndexPair.first);

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &bufferIndexPair.first;

				vkQueueSubmit(renderContext.graphicsPipeline.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
				vkQueueWaitIdle(renderContext.graphicsPipeline.GetGraphicsQueue());
				vkResetCommandBuffer(bufferIndexPair.first, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
				renderContext.commandPool.ReturnBuffer(bufferIndexPair);
			}
			
			/// @brief Transitions the GPU bound VkImage from its current layout into a new layout.
			void TransitionLayoutCmd(TinyVkImageLayout newLayout) {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = BeginTransferCmd();
				
				VkPipelineStageFlags srcStage, dstStage;
				VkImageMemoryBarrier pipelineBarrier = GetPipelineBarrier(newLayout, TinyVkCmdBufferSubmitStage::TINYVK_BEGIN_TO_END, srcStage, dstStage);
				imageLayout = newLayout;
				aspectFlags = pipelineBarrier.subresourceRange.aspectMask;
				vkCmdPipelineBarrier(bufferIndexPair.first, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &pipelineBarrier);

				EndTransferCmd(bufferIndexPair);
			}

			/// @brief Transitions the GPU bound VkImage from its current layout into a new layout.
			void TransitionLayoutBarrier(VkCommandBuffer cmdBuffer, TinyVkCmdBufferSubmitStage cmdBufferStage, TinyVkImageLayout newLayout) {
				VkPipelineStageFlags srcStage, dstStage;
				VkImageMemoryBarrier pipelineBarrier = GetPipelineBarrier(newLayout, cmdBufferStage, srcStage, dstStage);
				imageLayout = newLayout;
				aspectFlags = pipelineBarrier.subresourceRange.aspectMask;
				vkCmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &pipelineBarrier);
			}

			/// @brief Copies data from CPU accessible memory to GPU accessible memory.
			void StageImageData(void* data, VkDeviceSize dataSize) {
				TinyVkBuffer stagingBuffer = TinyVkBuffer(renderContext, dataSize, TinyVkBufferType::TINYVK_BUFFER_TYPE_STAGING);
				
				memcpy(stagingBuffer.description.pMappedData, data, (size_t)dataSize);
				TransitionLayoutCmd(TinyVkImageLayout::TINYVK_TRANSFER_DST);
				TransferFromBufferCmd(stagingBuffer);
				TransitionLayoutCmd(TinyVkImageLayout::TINYVK_COLOR_ATTACHMENT);

				stagingBuffer.Dispose();
			}

			/// @brief Copies data from the source TinyVkBuffer into this TinyVkImage.
			void TransferFromBufferCmd(TinyVkBuffer& srcBuffer) {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = BeginTransferCmd();

				TransitionLayoutBarrier(bufferIndexPair.first, TinyVkCmdBufferSubmitStage::TINYVK_BEGIN_TO_END, imageLayout);
				VkBufferImageCopy region = {
					.bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
					.imageSubresource.mipLevel = 0, .imageSubresource.baseArrayLayer = 0, .imageSubresource.layerCount = 1,
					.imageSubresource.aspectMask = aspectFlags,
					.imageOffset = { 0, 0, 0 }, .imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 }
				};
				vkCmdCopyBufferToImage(bufferIndexPair.first, srcBuffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

				EndTransferCmd(bufferIndexPair);
			}

			/// @brief Copies data from the source TinyVkBuffer into this TinyVkImage.
			void TransferFromBufferCmdExt(TinyVkBuffer& srcBuffer, VkExtent2D size, VkOffset2D offset) {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = BeginTransferCmd();

				TransitionLayoutBarrier(bufferIndexPair.first, TinyVkCmdBufferSubmitStage::TINYVK_BEGIN_TO_END, imageLayout);
				VkBufferImageCopy region = {
					.bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
					.imageSubresource.mipLevel = 0, .imageSubresource.baseArrayLayer = 0, .imageSubresource.layerCount = 1,
					.imageSubresource.aspectMask = aspectFlags,
					.imageExtent = { static_cast<uint32_t>(size.width), static_cast<uint32_t>(size.height), 1 },
					.imageOffset = { static_cast<int32_t>(offset.x), static_cast<int32_t>(offset.y), 0 }
				};
				vkCmdCopyBufferToImage(bufferIndexPair.first, srcBuffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

				EndTransferCmd(bufferIndexPair);
			}
			
			/// @brief Copies data from this TinyVkImage into the destination TinyVkBuffer
			void TransferToBufferCmd(TinyVkBuffer& dstBuffer) {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = BeginTransferCmd();

				TransitionLayoutBarrier(bufferIndexPair.first, TinyVkCmdBufferSubmitStage::TINYVK_BEGIN_TO_END, imageLayout);
				VkBufferImageCopy region = {
					.bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
					.imageSubresource.mipLevel = 0, .imageSubresource.baseArrayLayer = 0, .imageSubresource.layerCount = 1,
					.imageSubresource.aspectMask = aspectFlags,
					.imageOffset = { 0, 0, 0 }, .imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 }
				};
				vkCmdCopyImageToBuffer(bufferIndexPair.first, image, (VkImageLayout) imageLayout, dstBuffer.buffer, 1, &region);

				EndTransferCmd(bufferIndexPair);
			}

			/// @brief Copies data from this TinyVkImage into the destination TinyVkBuffer
			void TransferToBufferCmdExt(TinyVkBuffer& dstBuffer, VkExtent2D size, VkOffset2D offset) {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = BeginTransferCmd();

				TransitionLayoutBarrier(bufferIndexPair.first, TinyVkCmdBufferSubmitStage::TINYVK_BEGIN_TO_END, imageLayout);
				VkBufferImageCopy region{};
				region.bufferOffset = 0;
				region.bufferRowLength = 0;
				region.bufferImageHeight = 0;
				region.imageSubresource.aspectMask = aspectFlags;
				region.imageSubresource.mipLevel = 0;
				region.imageSubresource.baseArrayLayer = 0;
				region.imageSubresource.layerCount = 1;
				region.imageExtent = { static_cast<uint32_t>(size.width), static_cast<uint32_t>(size.height), 1 };
				region.imageOffset = { static_cast<int32_t>(offset.x), static_cast<int32_t>(offset.y), 0 };
				vkCmdCopyImageToBuffer(bufferIndexPair.first, image, (VkImageLayout) imageLayout, dstBuffer.buffer, 1, &region);

				EndTransferCmd(bufferIndexPair);
			}

			/// @brief Copies data from this TinyVkImage into the destination TinyVkBuffer
			inline static void TransferImageCmd(TinyVkRenderContext& renderContext, TinyVkImage& srcImage, TinyVkImage& dstImage, VkDeviceSize dataSize) {
				if (srcImage.format != dstImage.format)
					throw TinyVkRuntimeError("TinyVulkan: Tried to copy [SOURCE] image to [DESTINATION] image with different VkImageFormat!");
				
				TinyVkBuffer buffer(renderContext, dataSize, TinyVkBufferType::TINYVK_BUFFER_TYPE_STAGING);
				srcImage.TransferToBufferCmd(buffer);
				dstImage.TransferFromBufferCmd(buffer);
			}

			/// @brief Copies data from this TinyVkImage into the destination TinyVkBuffer
			inline static void TransferImageCmdExt(TinyVkRenderContext& renderContext, TinyVkImage& srcImage, TinyVkImage& dstImage, VkDeviceSize dataSize, VkExtent2D size, VkOffset2D srcOffset, VkOffset2D dstOffset) {
				if (srcImage.format != dstImage.format)
					throw TinyVkRuntimeError("TinyVulkan: Tried to copy [SOURCE] image to [DESTINATION] image with different VkImageFormat!");
				
				TinyVkBuffer buffer(renderContext, dataSize, TinyVkBufferType::TINYVK_BUFFER_TYPE_STAGING);
				srcImage.TransferToBufferCmdExt(buffer, size, srcOffset);
				dstImage.TransferFromBufferCmdExt(buffer, size, dstOffset);
			}
			
			#pragma endregion
			#pragma region PIPELINE_DESCRIPTOR
			
			/// @brief Creates the data descriptor that represents this image when passing into graphicspipeline.SelectWrite*Descriptor().
			VkDescriptorImageInfo GetImageDescriptor() { return { imageSampler, imageView, (VkImageLayout) imageLayout }; }

			#pragma endregion
			#pragma region COORDINATE_MATH
			
			/// @brief Returns a vec2 UV coordinate converted from this image Width/Height and passed vec2 XY coordinate.
			glm::vec2 GetUVCoords(glm::vec2 xy, bool forceClamp = true) {
				if (forceClamp)
					xy = glm::clamp(xy, glm::vec2(0.0,0.0), glm::vec2(static_cast<float>(width), static_cast<float>(height)));
				
				return glm::vec2(xy.x * (1.0 / static_cast<float>(width)), xy.y * (1.0 / static_cast<float>(height)));
			}

			/// @brief Returns a vec2 XY coordinate converted from this image Width/Height and passed vec2 UV coordinate.
			glm::vec2 GetXYCoords(glm::vec2 uv, bool forceClamp = true) {
				if (forceClamp)
					uv = glm::clamp(uv, glm::vec2(0.0, 0.0), glm::vec2(1.0, 1.0));
					
				return glm::vec2(uv.x * static_cast<float>(width), uv.y * static_cast<float>(height));
			}

			#pragma endregion
		};
	}
#endif