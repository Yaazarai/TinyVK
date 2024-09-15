#pragma once
#ifndef TINYVK_TINYVKIMAGERENDERER
#define TINYVK_TINYVKIMAGERENDERER
	#include "./TinyVulkan.hpp"

	namespace TINYVULKAN_NAMESPACE {
		/// 
		/// RENDERING PARADIGM:
		///		TinyVkImageRenderer is for rendering directly to a VkImage render target for offscreen rendering.
		///		Call RenderExecute(mutex[optional], preRecordedCommandBuffer[optiona]) to render to the image.
		///			You may pass a pre-recorded command buffer ready to go, or retrieve a command buffer from
		///			the underlying command pool queue and build your command buffer with a render event (onRenderEvent).
		///				onRenderEvent.hook(callback<VkCommandBuffer>(...));
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

		/// <summary>Offscreen Rendering (Render-To-Texture Model): Render to VkImage.</summary>
		class TinyVkImageRenderer : public TinyVkRendererInterface /*TinyVk_Utilities.hpp*/, TinyVkDisposable {
		private:
			TinyVkImage* optionalDepthImage;
			TinyVkRenderContext* depthImageRenderContext;
			TinyVkImage* renderTarget;

		public:
			TinyVkRenderContext& renderContext;
			TinyVkCommandPool commandPool;

			/// Invokable Render Events: (executed in TinyVkImageRenderer::RenderExecute()
			TinyVkInvokable<TinyVkCommandPool&> onRenderEvents;

			~TinyVkImageRenderer() { this->Dispose(); }

			void Disposable(bool waitIdle) {
				if (waitIdle) vkDeviceWaitIdle(renderContext.vkdevice.GetLogicalDevice());

				commandPool.Dispose();

				if (renderContext.graphicsPipeline.DepthTestingIsEnabled()) {
					optionalDepthImage->Dispose();
					delete optionalDepthImage;
				}
			}

			/// <summary>Creates a headless renderer specifically for performing render commands on a TinyVkImage (VkImage).</summary>
			TinyVkImageRenderer(TinyVkRenderContext& renderContext, TinyVkImage* renderTarget, size_t cmdpoolbuffercount = 32ULL)
			: commandPool(renderContext.vkdevice, cmdpoolbuffercount + 1), renderContext(renderContext), renderTarget(renderTarget) {
				onDispose.hook(TinyVkCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));

				optionalDepthImage = VK_NULL_HANDLE;
				if (renderContext.graphicsPipeline.DepthTestingIsEnabled()) {
					depthImageRenderContext = new TinyVkRenderContext(renderContext.vkdevice, commandPool, renderContext.graphicsPipeline);
					optionalDepthImage = new TinyVkImage(*depthImageRenderContext, renderTarget->width, renderTarget->height, true, renderContext.graphicsPipeline.QueryDepthFormat(), TINYVK_DEPTHSTENCIL_ATTACHMENT_OPTIMAL, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_IMAGE_ASPECT_DEPTH_BIT);
				}
			}

			TinyVkImageRenderer operator=(const TinyVkImageRenderer& imageRenderer) = delete;

			/// <summary>Sets the target image/texture for the TinyVkImageRenderer.</summary>
			void SetRenderTarget(TinyVkImage* renderTarget, bool waitOldTarget = true) {
				if (this->renderTarget != VK_NULL_HANDLE && waitOldTarget) {
					vkWaitForFences(renderContext.vkdevice.GetLogicalDevice(), 1, &renderTarget->imageWaitable, VK_TRUE, UINT64_MAX);
					vkResetFences(renderContext.vkdevice.GetLogicalDevice(), 1, &renderTarget->imageWaitable);
				}

				this->renderTarget = renderTarget;
			}

			/// <summary>Begins recording render commands to the provided command buffer.</summary>
			void BeginRecordCmdBuffer(VkCommandBuffer commandBuffer, const VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f }, const VkClearValue depthStencil = { .depthStencil = { 1.0f, 0 } }) {
				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
				beginInfo.pInheritanceInfo = VK_NULL_HANDLE; // Optional

				if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to record [begin] to command buffer!");

				const VkImageMemoryBarrier memory_barrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderTarget->image,
					.subresourceRange = {
					  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					  .baseMipLevel = 0,
					  .levelCount = 1,
					  .baseArrayLayer = 0,
					  .layerCount = 1,
					},
				};

				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &memory_barrier);

				VkRenderingAttachmentInfoKHR colorAttachmentInfo{};
				colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
				colorAttachmentInfo.imageView = renderTarget->imageView;
				colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				colorAttachmentInfo.clearValue = clearColor;

				VkRenderingInfoKHR dynamicRenderInfo{};
				dynamicRenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;

				VkRect2D renderAreaKHR{};
				renderAreaKHR.extent = { static_cast<uint32_t>(renderTarget->width), static_cast<uint32_t>(renderTarget->height) };
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
						.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.image = optionalDepthImage->image,
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
					depthStencilAttachmentInfo.imageView = optionalDepthImage->imageView;
					depthStencilAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					depthStencilAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
					depthStencilAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
					depthStencilAttachmentInfo.clearValue = depthStencil;
					dynamicRenderInfo.pDepthAttachment = &depthStencilAttachmentInfo;
				}

				VkViewport dynamicViewportKHR{};
				dynamicViewportKHR.x = 0;
				dynamicViewportKHR.y = 0;
				dynamicViewportKHR.width = static_cast<float>(renderTarget->width);
				dynamicViewportKHR.height = static_cast<float>(renderTarget->height);
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

				const VkImageMemoryBarrier image_memory_barrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderTarget->image,
					.subresourceRange = {
					  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					  .baseMipLevel = 0,
					  .levelCount = 1,
					  .baseArrayLayer = 0,
					  .layerCount = 1,
					}
				};

				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &image_memory_barrier);

				if (renderContext.graphicsPipeline.DepthTestingIsEnabled()) {
					const VkImageMemoryBarrier depth_memory_barrier{
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
						.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
						.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
						.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.image = optionalDepthImage->image,
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
			
			/// <summary>Executes the registered onRenderEvents and renders them to the target image/texture.</summary>
			void RenderExecute(/* VkCommandBuffer preRecordedCmdBuffer = VK_NULL_HANDLE */) {
				timed_guard imageLock(renderTarget->image_lock);
				if (!imageLock.Acquired()) return;
				
				if (renderTarget == VK_NULL_HANDLE)
					throw new std::runtime_error("TinyVulkan: RenderTarget for TinyVkImageRenderer is not set [VK_NULL_HANDLE]!");

				vkWaitForFences(renderContext.vkdevice.GetLogicalDevice(), 1, &renderTarget->imageWaitable, VK_TRUE, UINT64_MAX);
				vkResetFences(renderContext.vkdevice.GetLogicalDevice(), 1, &renderTarget->imageWaitable);
				
				//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				if (renderContext.graphicsPipeline.DepthTestingIsEnabled()) {
					TinyVkImage* depthImage = optionalDepthImage;
					if (depthImage->width != renderTarget->width || depthImage->height != renderTarget->height) {
						depthImage->Disposable(false);
						depthImage->ReCreateImage(renderTarget->width, renderTarget->height, depthImage->isDepthImage, renderContext.graphicsPipeline.QueryDepthFormat(), TINYVK_DEPTHSTENCIL_ATTACHMENT_OPTIMAL, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_ASPECT_DEPTH_BIT);
					}
				}
				
				vkResetCommandPool(renderContext.vkdevice.GetLogicalDevice(), commandPool.GetPool(), 0);
				onRenderEvents.invoke(commandPool);
				//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

				std::vector<VkCommandBuffer> commandBuffers;
				auto buffers = commandPool.GetBuffers();
				std::for_each(buffers.begin(), buffers.end(),
					[&commandBuffers](std::pair<VkCommandBuffer, VkBool32> cmdBuffer){
						if (cmdBuffer.second) commandBuffers.push_back(cmdBuffer.first);
				});

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
				submitInfo.pCommandBuffers = commandBuffers.data();
				
				VkSemaphore signalSemaphores[] = { renderTarget->imageFinished };
				submitInfo.signalSemaphoreCount = 1;
				submitInfo.pSignalSemaphores = signalSemaphores;

				if (vkQueueSubmit(renderContext.graphicsPipeline.GetGraphicsQueue(), 1, &submitInfo, renderTarget->imageWaitable) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to submit draw command buffer!");
			}
		};
	}
#endif