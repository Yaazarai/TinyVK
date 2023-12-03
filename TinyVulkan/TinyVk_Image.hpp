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

		enum TinyVkImageLayout {
			TINYVK_TRANSFER_SRC_OPTIMAL = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			TINYVK_TRANSFER_DST_OPTIMAL = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			TINYVK_SHADER_READONLY_OPTIMAL = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			TINYVK_DEPTHSTENCIL_ATTACHMENT_OPTIMAL = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			TINYVK_UNDEFINED = VK_IMAGE_LAYOUT_UNDEFINED,
			TINYVK_COLOR_ATTACHMENT_OPTIMAL = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		/// <summary>GPU device image for sending images to the render (GPU) device.</summary>
		class TinyVkImage : public TinyVkDisposable {
		private:
			TinyVkVulkanDevice& vkdevice;
			TinyVkGraphicsPipeline& graphicsPipeline;
			TinyVkCommandPool& commandPool;
			
			VkSamplerAddressMode addressingMode;

			void CreateImageView() {
				VkImageViewCreateInfo createInfo{};
				createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				createInfo.image = image;
				createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				createInfo.format = format;
				
				createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

				createInfo.subresourceRange.aspectMask = aspectFlags;
				createInfo.subresourceRange.baseMipLevel = 0;
				createInfo.subresourceRange.levelCount = 1;
				createInfo.subresourceRange.baseArrayLayer = 0;
				createInfo.subresourceRange.layerCount = 1;

				if (vkCreateImageView(vkdevice.GetLogicalDevice(), &createInfo, VK_NULL_HANDLE, &imageView) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to create TinyVkImage view!");
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
				
				VkPhysicalDeviceProperties properties{};
				vkGetPhysicalDeviceProperties(vkdevice.GetPhysicalDevice(), &properties);
				samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

				samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
				samplerInfo.unnormalizedCoordinates = VK_FALSE;
				samplerInfo.compareEnable = VK_FALSE;
				samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				samplerInfo.mipLodBias = 0.0f;
				samplerInfo.minLod = 0.0f;
				samplerInfo.maxLod = 0.0f;

				if (vkCreateSampler(vkdevice.GetLogicalDevice(), &samplerInfo, VK_NULL_HANDLE, &imageSampler) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to create image texture sampler!");
			}

			void CreateImageSyncObjects() {
				VkSemaphoreCreateInfo semaphoreInfo{};
				semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

				VkFenceCreateInfo fenceInfo{};
				fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
				fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

				if (vkCreateSemaphore(vkdevice.GetLogicalDevice(), &semaphoreInfo, VK_NULL_HANDLE, &imageAvailable) != VK_SUCCESS ||
					vkCreateSemaphore(vkdevice.GetLogicalDevice(), &semaphoreInfo, VK_NULL_HANDLE, &imageFinished) != VK_SUCCESS ||
					vkCreateFence(vkdevice.GetLogicalDevice(), &fenceInfo, VK_NULL_HANDLE, &imageWaitable) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to create synchronization objects for a image renderer!");
			}
			
		public:
			std::timed_mutex image_lock;

			VmaAllocation memory = VK_NULL_HANDLE;
			VkImage image = VK_NULL_HANDLE;
			VkImageView imageView = VK_NULL_HANDLE;
			VkSampler imageSampler = VK_NULL_HANDLE;
			TinyVkImageLayout currentLayout;
			VkImageAspectFlags aspectFlags;

			VkSemaphore imageAvailable;
			VkSemaphore imageFinished;
			VkFence imageWaitable;

			VkDeviceSize width, height;
			VkFormat format;
			bool isDepthImage = false;

			~TinyVkImage() { this->Dispose(); }

			void Disposable(bool waitIdle) {
				if (waitIdle) vkdevice.DeviceWaitIdle();

				vkDestroySampler(vkdevice.GetLogicalDevice(), imageSampler, VK_NULL_HANDLE);
				vkDestroyImageView(vkdevice.GetLogicalDevice(), imageView, VK_NULL_HANDLE);
				vmaDestroyImage(vkdevice.GetAllocator(), image, memory);

				vkDestroySemaphore(vkdevice.GetLogicalDevice(), imageAvailable, VK_NULL_HANDLE);
				vkDestroySemaphore(vkdevice.GetLogicalDevice(), imageFinished, VK_NULL_HANDLE);
				vkDestroyFence(vkdevice.GetLogicalDevice(), imageWaitable, VK_NULL_HANDLE);
			}

			/// <summary>Creates a VkImage for rendering or loading image files (stagedata) into.</summary>
			TinyVkImage(TinyVkVulkanDevice& vkdevice, TinyVkGraphicsPipeline& graphicsPipeline, TinyVkCommandPool& commandPool, VkDeviceSize width, VkDeviceSize height, bool isDepthImage = false, VkFormat format = VK_FORMAT_B8G8R8A8_SRGB, TinyVkImageLayout layout = TINYVK_UNDEFINED, VkSamplerAddressMode addressingMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT)
			: vkdevice(vkdevice), graphicsPipeline(graphicsPipeline), commandPool(commandPool), width(width), height(height), isDepthImage(isDepthImage), format(format), currentLayout(TINYVK_UNDEFINED), addressingMode(addressingMode), aspectFlags(aspectFlags) {
				onDispose.hook(TinyVkCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));

				ReCreateImage(width, height, isDepthImage, format, layout, addressingMode, aspectFlags);
			}

			TinyVkImage operator=(const TinyVkImage& image) = delete;

			/// <summary>Recreates this TinyVkImage using a new layout/format (don't forget to call image.Disposable(bool waitIdle) to dispose of the previous image first.</summary>
			void ReCreateImage(VkDeviceSize width, VkDeviceSize height, bool isDepthImage = false, VkFormat format = VK_FORMAT_B8G8R8A8_SRGB, TinyVkImageLayout layout = TINYVK_UNDEFINED, VkSamplerAddressMode addressingMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT) {
				VkImageCreateInfo imgCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
				imgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
				imgCreateInfo.extent.width = static_cast<uint32_t>(width);
				imgCreateInfo.extent.height = static_cast<uint32_t>(height);
				imgCreateInfo.extent.depth = 1;
				imgCreateInfo.mipLevels = 1;
				imgCreateInfo.arrayLayers = 1;
				imgCreateInfo.format = format;
				imgCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
				currentLayout = TINYVK_UNDEFINED;
				this->width = width;
				this->height = height;

				if (!isDepthImage) {
					imgCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
				} else {
					imgCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
					layout = TINYVK_DEPTHSTENCIL_ATTACHMENT_OPTIMAL;
				}

				VmaAllocationCreateInfo allocCreateInfo = {};
				allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
				allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
				allocCreateInfo.priority = 1.0f;
				
				if (vmaCreateImage(vkdevice.GetAllocator(), &imgCreateInfo, &allocCreateInfo, &image, &memory, VK_NULL_HANDLE) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Could not allocate GPU image data for TinyVkImage!");

				CreateImageSyncObjects();
				CreateTextureSampler();
				CreateImageView();
				
				if (layout != TINYVK_UNDEFINED)
					TransitionLayoutCmd(layout);
			}

			/// <summary>Transitions the GPU bound VkImage from its current layout into a new layout.</summary>
			void TransitionLayoutCmd(TinyVkImageLayout newLayout) {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = BeginTransferCmd();

				VkImageMemoryBarrier barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.oldLayout = (VkImageLayout) currentLayout;
				barrier.newLayout = (VkImageLayout) newLayout;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = image;
				barrier.subresourceRange.aspectMask = aspectFlags;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;

				VkPipelineStageFlags sourceStage;
				VkPipelineStageFlags destinationStage;

				switch ((VkImageLayout) currentLayout) {
					case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
						barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
					break;
					case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
						barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
					break;
					case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
						barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
						sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
					break;
					case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
						barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
						sourceStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
					break;
					case VK_IMAGE_LAYOUT_UNDEFINED:
					default:
						barrier.srcAccessMask = VK_ACCESS_NONE;
						sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					break;
				}

				switch ((VkImageLayout) newLayout) {
					case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
						barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
					break;
					case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
						barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
					break;
					case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
						barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
						destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
					break;
					case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
						barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
						barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
						destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

						if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT)
							barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
					break;
					case VK_IMAGE_LAYOUT_UNDEFINED:
					default:
						barrier.dstAccessMask = VK_ACCESS_NONE;
						destinationStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					break;
				}

				currentLayout = newLayout;
				vkCmdPipelineBarrier(bufferIndexPair.first, sourceStage, destinationStage, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);

				EndTransferCmd(bufferIndexPair);
			}

			/// <summary>Copies data from CPU accessible memory to GPU accessible memory.</summary>
			void StageImageData(void* data, VkDeviceSize dataSize) {
				TinyVkBuffer stagingBuffer = TinyVkBuffer(vkdevice, graphicsPipeline, commandPool, dataSize, TinyVkBufferType::VKVMA_BUFFER_TYPE_STAGING);
				memcpy(stagingBuffer.description.pMappedData, data, (size_t)dataSize);

				TransitionLayoutCmd(TINYVK_TRANSFER_DST_OPTIMAL);
				TransferFromBufferCmd(stagingBuffer);
				TransitionLayoutCmd(TINYVK_SHADER_READONLY_OPTIMAL);

				stagingBuffer.Dispose();
			}

			/// <summary>Copies data from the source TinyVkBuffer into this TinyVkImage.</summary>
			void TransferFromBufferCmd(TinyVkBuffer& srcBuffer) {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = BeginTransferCmd();

				VkBufferImageCopy region{};
				region.bufferOffset = 0;
				region.bufferRowLength = 0;
				region.bufferImageHeight = 0;
				region.imageSubresource.aspectMask = aspectFlags;
				region.imageSubresource.mipLevel = 0;
				region.imageSubresource.baseArrayLayer = 0;
				region.imageSubresource.layerCount = 1;
				region.imageOffset = { 0, 0, 0 };
				region.imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
				vkCmdCopyBufferToImage(bufferIndexPair.first, srcBuffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

				EndTransferCmd(bufferIndexPair);
			}

			/// <summary>Copies data from this TinyVkImage into the destination TinyVkBuffer</summary>
			void TransferToBufferCmd(TinyVkBuffer& dstBuffer) {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = BeginTransferCmd();

				VkBufferImageCopy region{};
				region.bufferOffset = 0;
				region.bufferRowLength = 0;
				region.bufferImageHeight = 0;
				region.imageSubresource.aspectMask = aspectFlags;
				region.imageSubresource.mipLevel = 0;
				region.imageSubresource.baseArrayLayer = 0;
				region.imageSubresource.layerCount = 1;
				region.imageOffset = { 0, 0, 0 };
				region.imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
				vkCmdCopyImageToBuffer(bufferIndexPair.first, image, (VkImageLayout) currentLayout, dstBuffer.buffer, 1, &region);
				EndTransferCmd(bufferIndexPair);
			}

			/// <summary>Begins a transfer command and returns the command buffer index pair used for the command allocated from a TinyVkCommandPool.</summary>
			std::pair<VkCommandBuffer, int32_t> BeginTransferCmd() {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = commandPool.LeaseBuffer();

				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
				vkBeginCommandBuffer(bufferIndexPair.first, &beginInfo);
				return bufferIndexPair;
			}

			/// <summary>Ends a transfer command and gives the leased/rented command buffer pair back to the TinyVkCommandPool.</summary>
			void EndTransferCmd(std::pair<VkCommandBuffer, int32_t> bufferIndexPair) {
				vkEndCommandBuffer(bufferIndexPair.first);

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &bufferIndexPair.first;

				vkQueueSubmit(graphicsPipeline.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
				vkQueueWaitIdle(graphicsPipeline.GetGraphicsQueue());
				commandPool.ReturnBuffer(bufferIndexPair);
			}

			/// <summary>Creates the data descriptor that represents this image when passing into graphicspipeline.SelectWrite*Descriptor().</summary>
			VkDescriptorImageInfo GetImageDescriptor() { return { imageSampler, imageView, (VkImageLayout) currentLayout }; }

			/// <summary>Returns a vec2 UV coordinate converted from this image Width/Height and passed vec2 XY coordinate.</summary>
			glm::vec2 GetUVCoords(glm::vec2 xy, bool forceClamp = true) {
				if (forceClamp)
					xy = glm::clamp(xy, glm::vec2(0.0,0.0), glm::vec2(static_cast<float>(width), static_cast<float>(height)));
				
				return glm::vec2(xy.x * (1.0 / static_cast<float>(width)), xy.y * (1.0 / static_cast<float>(height)));
			}

			/// <summary>Returns a vec2 XY coordinate converted from this image Width/Height and passed vec2 UV coordinate.</summary>
			glm::vec2 GetXYCoords(glm::vec2 uv, bool forceClamp = true) {
				if (forceClamp)
					uv = glm::clamp(uv, glm::vec2(0.0, 0.0), glm::vec2(1.0, 1.0));
					
				return glm::vec2(uv.x * static_cast<float>(width), uv.y * static_cast<float>(height));
			}
		};
	}
#endif