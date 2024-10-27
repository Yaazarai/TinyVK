#pragma once
#ifndef TINYVK_TINYVCGRAPHICSNDERER
#define TINYVK_TINYVCGRAPHICSNDERER
	#include "./TinyVulkan.hpp"

	namespace TINYVULKAN_NAMESPACE {
		/// 
		/// RENDERING PARADIGM:
		///		TinyVkGraphicsRenderer is for rendering directly to a VkImage render target for offscreen rendering.
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

		/// @brief Offscreen Rendering (Render-To-Texture Model): Render to VkImage.
		class TinyVkGraphicsRenderer : public TinyVkRendererInterface {
		protected:
			TinyVkImage* optionalDepthImage;
			TinyVkImage* renderTarget;
			TinyVkCommandPool* commandPool;

		public:
			TinyVkRenderContext& renderContext;

            /// Invokable Render Events: (executed in TinyVkGraphicsRenderer::RenderExecute()
			TinyVkInvokable<TinyVkCommandPool&> onRenderEvents;

			/// @brief Deletes the copy-constructor (dynamic resources cannot be copied).
			TinyVkGraphicsRenderer operator=(const TinyVkGraphicsRenderer& renderer) = delete;
            
            /// @brief Simple render-to-image graphics pipeline renderer.
			TinyVkGraphicsRenderer(TinyVkRenderContext& renderContext, TinyVkCommandPool* cmdPool, TinyVkImage* renderTarget, TinyVkImage* optionalDepthImage = VK_NULL_HANDLE)
            : renderContext(renderContext), commandPool(cmdPool), renderTarget(renderTarget), optionalDepthImage(optionalDepthImage) {
                if (renderContext.graphicsPipeline.DepthTestingIsEnabled() && optionalDepthImage == VK_NULL_HANDLE)
                    throw TinyVkRuntimeError("TinyVulkan: Trying to create TinyVkGraphicsRenderer without depth image [VK_NULL_HANDLE]! on depth testing enabled graphics pipeline!");
            }

			#pragma region RENDER_TARGETING
			
			/// @brief Sets the target image/texture for the TinyVkImageRenderer.
			void SetRenderTarget(TinyVkCommandPool* cmdPool, TinyVkImage* renderTarget, TinyVkImage* optionalDepthImage = VK_NULL_HANDLE, bool waitOldTarget = true) {
				if (this->renderTarget != VK_NULL_HANDLE && waitOldTarget) {
					vkWaitForFences(renderContext.vkdevice.GetLogicalDevice(), 1, &renderTarget->imageWaitable, VK_TRUE, UINT64_MAX);
					vkResetFences(renderContext.vkdevice.GetLogicalDevice(), 1, &renderTarget->imageWaitable);
				}

                if (renderContext.graphicsPipeline.DepthTestingIsEnabled() && optionalDepthImage == VK_NULL_HANDLE)
                    throw TinyVkRuntimeError("TinyVulkan: Trying to reset render target on TinyVkGraphicsRenderer without depth image on depth testing enabled graphics pipeline!");
                
                this->commandPool = cmdPool;
				this->renderTarget = renderTarget;
                this->optionalDepthImage = optionalDepthImage;
			}

			#pragma endregion
			#pragma region PIPELINE_DESCRIPTORS
			
			/// @brief Records Push Constants to the command buffer.
			void PushConstants(VkCommandBuffer cmdBuffer, VkShaderStageFlagBits shaderFlags, uint32_t byteSize, const void* pValues) {
				vkCmdPushConstants(cmdBuffer, renderContext.graphicsPipeline.GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, byteSize, pValues);
			}

			/// @brief Records Push Descriptors to the command buffer.
			VkResult PushDescriptorSet(VkCommandBuffer cmdBuffer, std::vector<VkWriteDescriptorSet> writeDescriptorSets) {
				return vkCmdPushDescriptorSetEKHR(renderContext.vkdevice.GetInstance(), cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderContext.graphicsPipeline.GetPipelineLayout(),
					0, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data());
			}
            
            #pragma endregion
			#pragma region RENDERING_COMMAND_RECORDING
			
			/// @brief Begins recording render commands to the provided command buffer.
			void BeginRecordCmdBuffer(VkCommandBuffer commandBuffer, std::vector<TinyVkImage*> syncImages = {}, std::vector<TinyVkBuffer*> syncBuffers = {}, const VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f }, const VkClearValue depthStencil = { .depthStencil = { 1.0f, 0 } }) {
				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
				beginInfo.pInheritanceInfo = VK_NULL_HANDLE; // Optional

				if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
					throw TinyVkRuntimeError("TinyVulkan: Failed to record [begin] to command buffer!");
                
                renderTarget->TransitionLayoutBarrier(commandBuffer, TinyVkCmdBufferSubmitStage::TINYVK_BEGIN, TinyVkImageLayout::TINYVK_COLOR_ATTACHMENT);

				VkRenderingAttachmentInfoKHR colorAttachmentInfo{};
				colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
				colorAttachmentInfo.imageView = renderTarget->imageView;
				colorAttachmentInfo.imageLayout = (VkImageLayout) renderTarget->imageLayout;
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
                    if (optionalDepthImage == VK_NULL_HANDLE)
                        throw TinyVkRuntimeError("TinyVulkan: Trying to render with TinyVkGraphicsRenderer without depth image [VK_NULL_HANDLE]! on depth testing enabled graphics pipeline!");
					
                    optionalDepthImage->TransitionLayoutBarrier(commandBuffer, TinyVkCmdBufferSubmitStage::TINYVK_BEGIN, TinyVkImageLayout::TINYVK_DEPTHSTENCIL_ATTACHMENT);

                    depthStencilAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
					depthStencilAttachmentInfo.imageView = optionalDepthImage->imageView;
					depthStencilAttachmentInfo.imageLayout = (VkImageLayout) optionalDepthImage->imageLayout;
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
					throw TinyVkRuntimeError("TinyVulkan: Failed to record [begin] to rendering!");
				
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderContext.graphicsPipeline.GetGraphicsPipeline());
			}

			/// @brief Ends recording render commands to the provided command buffer.
			void EndRecordCmdBuffer(VkCommandBuffer commandBuffer, std::vector<TinyVkImage*> syncImages = {}, std::vector<TinyVkBuffer*> syncBuffers = {}, const VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f }, const VkClearValue depthStencil = { .depthStencil = { 1.0f, 0 } }) {
				if (vkCmdEndRenderingEKHR(renderContext.vkdevice.GetInstance(), commandBuffer) != VK_SUCCESS)
					throw TinyVkRuntimeError("TinyVulkan: Failed to record [end] to rendering!");

				renderTarget->TransitionLayoutBarrier(commandBuffer, TinyVkCmdBufferSubmitStage::TINYVK_END, (renderTarget->imageType == TinyVkImageType::TINYVK_IMAGE_TYPE_SWAPCHAIN)? TinyVkImageLayout::TINYVK_PRESENT_SRC : TinyVkImageLayout::TINYVK_COLOR_ATTACHMENT);

				if (renderContext.graphicsPipeline.DepthTestingIsEnabled()) {
                    if (optionalDepthImage == VK_NULL_HANDLE)
                        throw TinyVkRuntimeError("TinyVulkan: Trying to render with TinyVkGraphicsRenderer without depth image [VK_NULL_HANDLE] on depth testing enabled graphics pipeline!");
					
                    optionalDepthImage->TransitionLayoutBarrier(commandBuffer, TinyVkCmdBufferSubmitStage::TINYVK_END, TinyVkImageLayout::TINYVK_DEPTHSTENCIL_ATTACHMENT);
				}

				if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
					throw TinyVkRuntimeError("TinyVulkan: Failed to record [end] to command buffer!");
			}
			
			#pragma endregion
			#pragma region RENDERING_SUBMISSION_AND_EXECUTION

			/// @brief Executes the registered onRenderEvents and renders them to the target image/texture.
			virtual VkResult RenderExecute(bool waitFences = true) {
				if (renderTarget == VK_NULL_HANDLE)
                    throw TinyVkRuntimeError("TinyVulkan: RenderTarget for TinyVkImageRenderer is [VK_NULL_HANDLE]!");
				
				if (waitFences) {
					vkWaitForFences(renderContext.vkdevice.GetLogicalDevice(), 1, &renderTarget->imageWaitable, VK_TRUE, UINT64_MAX);
					vkResetFences(renderContext.vkdevice.GetLogicalDevice(), 1, &renderTarget->imageWaitable);
				}
				
				//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				if (renderContext.graphicsPipeline.DepthTestingIsEnabled()) {
                    if (optionalDepthImage == VK_NULL_HANDLE)
                        throw TinyVkRuntimeError("TinyVulkan: Trying to render with TinyVkGraphicsRenderer without depth image [VK_NULL_HANDLE]! on depth testing enabled graphics pipeline!");
                    
                    if (optionalDepthImage->width != renderTarget->width || optionalDepthImage->height != renderTarget->height) {
						optionalDepthImage->Disposable(false);
						optionalDepthImage->ReCreateImage(optionalDepthImage->imageType, renderTarget->width, renderTarget->height, renderContext.graphicsPipeline.QueryDepthFormat(), VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
					}
				}
				
				commandPool->ReturnAllBuffers();
                onRenderEvents.invoke(*commandPool);
				//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                
				std::vector<VkCommandBuffer> commandBuffers;
				auto buffers = commandPool->GetBuffers();
				std::for_each(buffers.begin(), buffers.end(),
					[&commandBuffers](std::pair<VkCommandBuffer, VkBool32> cmdBuffer){
						if (cmdBuffer.second) commandBuffers.push_back(cmdBuffer.first);
				});

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
				submitInfo.pCommandBuffers = commandBuffers.data();
				
				VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
				VkSemaphore waitSemaphores[] = { renderTarget->imageAvailable };
				VkSemaphore signalSemaphores[] = { renderTarget->imageFinished };
				
				VkResult result;
				if (renderTarget->imageType == TinyVkImageType::TINYVK_IMAGE_TYPE_SWAPCHAIN) {
					submitInfo.waitSemaphoreCount = 1;
					submitInfo.pWaitDstStageMask = waitStages;
					submitInfo.pWaitSemaphores = waitSemaphores;
					submitInfo.signalSemaphoreCount = 1;
					submitInfo.pSignalSemaphores = signalSemaphores;
					result = vkQueueSubmit(renderContext.graphicsPipeline.GetPresentQueue(), 1, &submitInfo, renderTarget->imageWaitable);
				} else {
					result = vkQueueSubmit(renderContext.graphicsPipeline.GetGraphicsQueue(), 1, &submitInfo, renderTarget->imageWaitable);
				}

				if (result != VK_SUCCESS)
					throw TinyVkRuntimeError("TinyVulkan: Failed to submit draw command buffer graphics queue!");
				return result;
			}

			/// @brief Acquires the target's mutex lock and executes the registered onRenderEvents and renders them to the target image/texture.
			VkResult RenderExecuteThreadSafe() {
				timed_guard imageLock(renderTarget->image_lock);
				if (!imageLock.Acquired()) {
					throw TinyVkRuntimeError("TinyVulkan: could not acquire lock for renderer! Running in another thread?");
					return VK_ERROR_OUT_OF_DATE_KHR;
				}
				return RenderExecute();
			}

			#pragma endregion
        };
    }
#endif

/*
    Vulkan Graphics Pipeline Order:
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT = 0x00000001,
		VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT = 0x00000002,
		VK_PIPELINE_STAGE_VERTEX_INPUT_BIT = 0x00000004,
		VK_PIPELINE_STAGE_VERTEX_SHADER_BIT = 0x00000008,
		VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT = 0x00000010,
		VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT = 0x00000020,
		VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT = 0x00000040,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT = 0x00000080,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT = 0x00000100,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT = 0x00000200,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = 0x00000400,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT = 0x00000800,
		VK_PIPELINE_STAGE_TRANSFER_BIT = 0x00001000,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = 0x00002000
	
	Top of Pipe
		Draw Indirect

		Vertex Input
		Vertex Shader

		Tessellation Shader
		Geometry Shader

		Fragment Shader
		Early Fragment Test
		Late Fragment Test
		Color Attachement Output

		Transfer
	Bottom of Pipe
*/