#pragma once
#ifndef TINYVK_TINYVCOMPUTERENDERER
#define TINYVK_TINYVCOMPUTERENDERER
	#include "./TinyVulkan.hpp"

	namespace TINYVULKAN_NAMESPACE {
		/// @brief Vulkan Compute Pipeline & Renderer using Storage Buffers/Images and Push Descriptors/Constants.
		class TinyVkComputeRenderer : public TinyVkDisposable {
        private:
			VkShaderModule CreateShaderModule(std::vector<char> shaderCode) {
				VkShaderModuleCreateInfo createInfo{};
				createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
				createInfo.pNext = VK_NULL_HANDLE;
				createInfo.flags = 0;
				createInfo.codeSize = shaderCode.size();
				createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

				VkShaderModule shaderModule;
				if (vkCreateShaderModule(vkdevice.GetLogicalDevice(), &createInfo, VK_NULL_HANDLE, &shaderModule) != VK_SUCCESS)
					throw TinyVkRuntimeError("TinyVulkan: Failed to create shader module!");

				return shaderModule;
			}

			VkPipelineShaderStageCreateInfo CreateShaderInfo(const std::string& path, VkShaderModule shaderModule, VkShaderStageFlagBits stageFlagBits) {
				VkPipelineShaderStageCreateInfo shaderStageInfo{};
				shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				shaderStageInfo.stage = stageFlagBits;
				shaderStageInfo.module = shaderModule;
				shaderStageInfo.pName = "main";

				#if TVK_VALIDATION_LAYERS
				std::cout << "TinyVulkan: Loading Shader @ " << path << std::endl;
				#endif

				return shaderStageInfo;
			}

			std::vector<char> ReadFile(const std::string& path) {
				std::ifstream file(path, std::ios::ate | std::ios::binary);

				if (!file.is_open())
					throw TinyVkRuntimeError("TinyVulkan: Failed to Read File: " + path);

				size_t fsize = static_cast<size_t>(file.tellg());
				std::vector<char> buffer(fsize);
				file.seekg(0);
				file.read(buffer.data(), fsize);
				file.close();
				return buffer;
			}
			
            void CreateComputePipeline(const std::string shader) {
				VkPipelineShaderStageCreateInfo shaderPipelineCreateInfo;
				VkShaderModule shaderModule;
				auto shaderCode = ReadFile(shader);
				shaderModule = CreateShaderModule(shaderCode);
				shaderPipelineCreateInfo = CreateShaderInfo(shader, shaderModule, VK_SHADER_STAGE_COMPUTE_BIT);

				///////////////////////////////////////////////////////////////////////////////////////////////////////
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
				pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
				
				pipelineLayoutInfo.pushConstantRangeCount = 0;
				uint32_t pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
				if (pushConstantRangeCount > 0) {
					pipelineLayoutInfo.pushConstantRangeCount = pushConstantRangeCount;
					pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
				}

				if (descriptorBindings.size() > 0) {
					VkDescriptorSetLayoutCreateInfo descriptorCreateInfo{};
					descriptorCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
					descriptorCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
					descriptorCreateInfo.bindingCount = static_cast<uint32_t>(descriptorBindings.size());
					descriptorCreateInfo.pBindings = descriptorBindings.data();

					if (vkCreateDescriptorSetLayout(vkdevice.GetLogicalDevice(), &descriptorCreateInfo, VK_NULL_HANDLE, &descriptorLayout) != VK_SUCCESS)
						throw TinyVkRuntimeError("TinyVulkan: Failed to create push descriptor bindings! ");

					pipelineLayoutInfo.setLayoutCount = 1;
					pipelineLayoutInfo.pSetLayouts = &descriptorLayout;
				}

				if (vkCreatePipelineLayout(vkdevice.GetLogicalDevice(), &pipelineLayoutInfo, VK_NULL_HANDLE, &computePipelineLayout) != VK_SUCCESS)
					throw TinyVkRuntimeError("TinyVulkan: Failed to create graphics pipeline layout!");
				
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				///////////////////////////////////////////////////////////////////////////////////////////////////////

                VkComputePipelineCreateInfo pipelineInfo;
                pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
                pipelineInfo.flags = VK_PIPELINE_CREATE_COLOR_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT;
                pipelineInfo.stage = shaderPipelineCreateInfo;
                pipelineInfo.layout = computePipelineLayout;
                pipelineInfo.basePipelineHandle;
                pipelineInfo.basePipelineIndex;
                
                if (vkCreateComputePipelines(vkdevice.GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, VK_NULL_HANDLE, &computePipeline) != VK_SUCCESS)
                    throw TinyVkRuntimeError("TinyVulkan: Failed to create compute pipeline!");
                
                vkDestroyShaderModule(vkdevice.GetLogicalDevice(), shaderModule, VK_NULL_HANDLE);
				QueryPhysicalDeviceLimits(vkdevice.GetPhysicalDevice());
            }

        public:
            TinyVkVulkanDevice& vkdevice;
			TinyVkCommandPool& commandPool;
			VkDescriptorSetLayout descriptorLayout;
			std::vector<VkDescriptorSetLayoutBinding> descriptorBindings;
			std::vector<VkPushConstantRange> pushConstantRanges;
			VkPipelineLayout computePipelineLayout;
			VkPipeline computePipeline;
			VkQueue computeQueue;
			uint32_t maxWorkGroups[3], maxSizeOfWorkGroups[3];

            /// Invokable Render Events: (executed in TinyVkComputeRenderer::RenderExecute()
			TinyVkInvokable<TinyVkCommandPool&> onRenderEvents;
			TinyVkComputeRenderer operator=(const TinyVkComputeRenderer& renderer) = delete;

			~TinyVkComputeRenderer() { this->Dispose(); }

			void Disposable(bool waitIdle) {
				if (waitIdle) vkdevice.DeviceWaitIdle();

				vkDestroyDescriptorSetLayout(vkdevice.GetLogicalDevice(), descriptorLayout, VK_NULL_HANDLE);
				vkDestroyPipeline(vkdevice.GetLogicalDevice(), computePipeline, VK_NULL_HANDLE);
				vkDestroyPipelineLayout(vkdevice.GetLogicalDevice(), computePipelineLayout, VK_NULL_HANDLE);
			}
            
            TinyVkComputeRenderer(TinyVkVulkanDevice& vkdevice, TinyVkCommandPool& commandPool, TinyVkVertexDescription vertexDescription, const std::string shader, const std::vector<VkDescriptorSetLayoutBinding>& descriptorBindings, const std::vector<VkPushConstantRange>& pushConstantRanges)
            : vkdevice(vkdevice), commandPool(commandPool) {
                CreateComputePipeline(shader);
				
				TinyVkQueueFamily indices = vkdevice.FindQueueFamilies();
				vkGetDeviceQueue(vkdevice.GetLogicalDevice(), indices.computeFamily, 0, &computeQueue);
            }

			void QueryPhysicalDeviceLimits(VkPhysicalDevice device) {
				VkPhysicalDeviceLimits deviceLimits {};
				VkPhysicalDeviceProperties2 properties {};
				properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
				vkGetPhysicalDeviceProperties2(device, &properties);

				maxWorkGroups[0] = properties.properties.limits.maxComputeWorkGroupCount[0];
				maxWorkGroups[2] = properties.properties.limits.maxComputeWorkGroupCount[1];
				maxWorkGroups[3] = properties.properties.limits.maxComputeWorkGroupCount[2];
				maxSizeOfWorkGroups[0] = properties.properties.limits.maxComputeWorkGroupSize[0];
				maxSizeOfWorkGroups[2] = properties.properties.limits.maxComputeWorkGroupSize[1];
				maxSizeOfWorkGroups[3] = properties.properties.limits.maxComputeWorkGroupSize[2];
			}
			
			#pragma region RENDERING_COMMAND_RECORDING

			/// @brief Begins recording render commands to the provided command buffer.
			void BeginRecordCmdBuffer(VkCommandBuffer commandBuffer, std::vector<TinyVkBuffer*> syncStorageBuffers, std::vector<TinyVkImage*> syncStorageImages, const VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f }, const VkClearValue depthStencil = { .depthStencil = { 1.0f, 0 } }) {
				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
				beginInfo.pInheritanceInfo = VK_NULL_HANDLE; // Optional

				if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
					throw TinyVkRuntimeError("TinyVulkan: Failed to record [begin] to command buffer!");
				
				for(TinyVkBuffer* buffer : syncStorageBuffers)
					buffer->MemoryPipelineBarrier(commandBuffer, TinyVkCmdBufferSubmitStage::TINYVK_BEGIN);
				
				for(TinyVkImage* image : syncStorageImages)
					image->TransitionLayoutBarrier(commandBuffer, TinyVkCmdBufferSubmitStage::TINYVK_BEGIN, TinyVkImageLayout::TINYVK_GENERAL);

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
			}

			/// @brief Ends recording render commands to the provided command buffer.
			void EndRecordCmdBuffer(VkCommandBuffer commandBuffer, std::vector<TinyVkBuffer*> syncStorageBuffers, std::vector<TinyVkImage*> syncStorageImages, const VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f }, const VkClearValue depthStencil = { .depthStencil = { 1.0f, 0 } }) {
				for(TinyVkBuffer* buffer : syncStorageBuffers)
					buffer->MemoryPipelineBarrier(commandBuffer, TinyVkCmdBufferSubmitStage::TINYVK_END);
				
				for(TinyVkImage* image : syncStorageImages)
					image->TransitionLayoutBarrier(commandBuffer, TinyVkCmdBufferSubmitStage::TINYVK_END, TinyVkImageLayout::TINYVK_GENERAL);
				
				if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
					throw TinyVkRuntimeError("TinyVulkan: Failed to record [end] to command buffer!");
			}
			
			#pragma endregion
			#pragma region PIPELINE_DESCRIPTORS

			/// @brief Records Push Descriptors to the command buffer.
			VkResult PushDescriptorSet(VkCommandBuffer cmdBuffer, std::vector<VkWriteDescriptorSet> writeDescriptorSets) {
				return vkCmdPushDescriptorSetEKHR(vkdevice.GetInstance(), cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout,
					0, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data());
			}

			/// @brief Records Push Constants to the command buffer.
			void PushConstants(VkCommandBuffer cmdBuffer, VkShaderStageFlagBits shaderFlags, uint32_t byteSize, const void* pValues) {
				vkCmdPushConstants(cmdBuffer, computePipelineLayout, VK_PIPELINE_BIND_POINT_COMPUTE, 0, byteSize, pValues);
			}
			
			#pragma endregion
			#pragma region THREAD_DISPATCHING

			/// @brief Dispatches X*Y*Z number of work-groups with the size of each work-group specified within the compute shader.
			void CmdispatchGroups(VkCommandBuffer commandBuffer, std::array<uint32_t,3> wgroups, std::array<uint32_t,3> basewg) {
				if (wgroups[0] > maxWorkGroups[0] || wgroups[1] > maxWorkGroups[1] || wgroups[2] > maxWorkGroups[2]) {
					std::ostringstream error;
					error << "TinyVulkan: Tried to Dispatch [" << wgroups[0] << ", " << wgroups[1] << ", " << wgroups[2] << "]";
					error << " Work Groups, however device limits are: " << maxWorkGroups[0] << "," << maxWorkGroups[1] << "," << maxWorkGroups[2];
					throw tinyvk::TinyVkRuntimeError(error.str());
				}
				
				vkCmdDispatchBase(commandBuffer, wgroups[0], wgroups[1], wgroups[2], basewg[0], basewg[1], basewg[2]);
			}
			
			#pragma endregion
			#pragma region RENDERING_SUBMISSION_AND_EXECUTION
			
			/// @brief Executes the registered onRenderEvents and renders them to the target storage buffer.
			VkResult ComputeExecute(bool waitFences = true, std::vector<TinyVkBuffer*> storageBuffers = {}, std::vector<TinyVkImage*> storageImages = {}) {
				std::vector<VkFence> fences;
				if (waitFences) {
					for(TinyVkBuffer* buffer : storageBuffers)
						fences.push_back(buffer->bufferWaitable);
					
					for(TinyVkImage* image : storageImages)
						fences.push_back(image->imageWaitable);
					
					vkWaitForFences(vkdevice.GetLogicalDevice(), fences.size(), fences.data(), VK_TRUE, UINT64_MAX);
				}
				
				//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				commandPool.ReturnAllBuffers();
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
				VkResult result = vkQueueSubmit(computeQueue, 1, &submitInfo, fences[0]);

				if (result != VK_SUCCESS)
					throw TinyVkRuntimeError("TinyVulkan: Failed to submit compute command buffer to compute queue!");
				return result;
			}

			#pragma endregion
        };
    }
#endif

/*
	Rendering with Compute:
		1. Pass storage image with: (tells the shader we can read/write to an image)
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
		2. Transition image layout to VK_IMAGE_LAYOUT_GENERAL via memory pipeline barrier (synchronization).
		3. Wait on used storage images/buffers fences (synchronization).
			* Not required if they are not being modified by other threads/queues.
		3. Dispatch compute commands into command buffer executed with image layout transition (if needed).
		4. Submit command buffer work to the compute pipeline's compute queue.
	
	Dispatching Groups:
		Compute Shaders dispatch threads, each thread performs some amount of work.
		Threads are dispatched in groups called "Work Groups."
		The size of each Work Group is defined within the compute shader (constant).
		The number of Work Groups is what we dispatch to perform said work.
		This makes it difficult to spawn an exact size Work Group unless the group is very large.
		However large Work Groups may not fully occupy the GPU and may be sub-optimal.
		Profile as needed for optimal performance.
*/