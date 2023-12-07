#pragma once
#ifndef TINYVK_TINYVGRAPHICSPIPELINE
#define TINYVK_TINYVGRAPHICSPIPELINE
	#include "./TinyVulkan.hpp"

	namespace TINYVULKAN_NAMESPACE {
		#define VKCOMP_RGBA VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		#define VKCOMP_BGRA VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_A_BIT

		enum TinyVkDescriptorTypes {
			TINYVK_DESCRIPTOR_IMAGE_SAMPLER = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			TINYVK_DESCRIPTOR_STORAGE_IMAGE = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			TINYVK_DESCRIPTOR_UNIFORM_BUFFER = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			TINYVK_DESCRIPTOR_STORAGE_BUFFER = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
		};

		/// <summary>Represents the Vertex shader layout data passing through the graphics pipeline.</summary>
		struct TinyVkVertexDescription {
			const VkVertexInputBindingDescription binding;
			const std::vector<VkVertexInputAttributeDescription> attributes;

			TinyVkVertexDescription(VkVertexInputBindingDescription binding, const std::vector<VkVertexInputAttributeDescription> attributes) : binding(binding), attributes(attributes) {}
		};

		/// <summary>Vulkan Graphics Pipeline using Dynamic Viewports/Scissors, Push Descriptors/Constants.</summary>
		class TinyVkGraphicsPipeline : public TinyVkDisposable {
		private:
			VkDescriptorSetLayout descriptorLayout;
			std::vector<VkDescriptorSetLayoutBinding> descriptorBindings;
			std::vector<VkPushConstantRange> pushConstantRanges;

			VkPipelineDynamicStateCreateInfo dynamicState;
			VkPipelineLayout pipelineLayout;
			VkPipeline graphicsPipeline;
			
			VkFormat imageFormat;
			VkColorComponentFlags colorComponentFlags = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			VkPipelineColorBlendAttachmentState colorBlendState;

			TinyVkVertexDescription vertexDescription;
			VkPrimitiveTopology vertexTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			VkPolygonMode polgyonTopology = VK_POLYGON_MODE_FILL;
			
			bool enableBlending;
			bool enableDepthTesting;
			VkQueue graphicsQueue;
			VkQueue presentQueue;
			
			VkShaderModule CreateShaderModule(std::vector<char> shaderCode) {
				VkShaderModuleCreateInfo createInfo{};
				createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
				createInfo.pNext = VK_NULL_HANDLE;
				createInfo.flags = 0;
				createInfo.codeSize = shaderCode.size();
				createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

				VkShaderModule shaderModule;
				if (vkCreateShaderModule(vkdevice.GetLogicalDevice(), &createInfo, VK_NULL_HANDLE, &shaderModule) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to create shader module!");

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
					throw std::runtime_error("TinyVulkan: Failed to Read File: " + path);

				size_t fsize = static_cast<size_t>(file.tellg());
				std::vector<char> buffer(fsize);
				file.seekg(0);
				file.read(buffer.data(), fsize);
				file.close();
				return buffer;
			}
			
			void CreateGraphicsPipeline(const std::vector<std::tuple<VkShaderStageFlagBits, std::string>> shaders) {
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				/////////// This section specifies that TinyVkVertex provides the vertex layout description ///////////
				const VkVertexInputBindingDescription bindingDescription = vertexDescription.binding;
				const std::vector<VkVertexInputAttributeDescription>& attributeDescriptions = vertexDescription.attributes;

				VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
				vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
				vertexInputInfo.vertexBindingDescriptionCount = 1;
				vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
				vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
				vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
				
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
						throw std::runtime_error("TinyVulkan: Failed to create push descriptor bindings! ");

					pipelineLayoutInfo.setLayoutCount = 1;
					pipelineLayoutInfo.pSetLayouts = &descriptorLayout;
				}

				if (vkCreatePipelineLayout(vkdevice.GetLogicalDevice(), &pipelineLayoutInfo, VK_NULL_HANDLE, &pipelineLayout) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to create graphics pipeline layout!");
				
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
				inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
				inputAssembly.topology = vertexTopology;
				inputAssembly.primitiveRestartEnable = VK_FALSE;

				VkPipelineViewportStateCreateInfo viewportState{};
				viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
				viewportState.viewportCount = 1;
				viewportState.scissorCount = 1;
				viewportState.flags = 0;

				VkPipelineRasterizationStateCreateInfo rasterizer{};
				rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
				rasterizer.depthClampEnable = VK_FALSE;
				rasterizer.rasterizerDiscardEnable = VK_FALSE;
				rasterizer.polygonMode = polgyonTopology;
				rasterizer.lineWidth = 1.0f;
				rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
				rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
				rasterizer.depthBiasEnable = VK_FALSE;

				VkPipelineMultisampleStateCreateInfo multisampling{};
				multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
				multisampling.sampleShadingEnable = VK_FALSE;
				multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

				VkPipelineColorBlendStateCreateInfo colorBlending{};
				colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
				colorBlending.logicOpEnable = VK_FALSE;
				colorBlending.logicOp = VK_LOGIC_OP_COPY;
				colorBlending.attachmentCount = 1;

				VkPipelineColorBlendAttachmentState blendDescription = colorBlendState;
				colorBlending.pAttachments = &blendDescription;
				colorBlending.blendConstants[0] = 0.0f;
				colorBlending.blendConstants[1] = 0.0f;
				colorBlending.blendConstants[2] = 0.0f;
				colorBlending.blendConstants[3] = 0.0f;

				const std::array<VkDynamicState, 2> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
				dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
				dynamicState.flags = 0;
				dynamicState.pDynamicStates = dynamicStateEnables.data();
				dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
				dynamicState.pNext = VK_NULL_HANDLE;

				VkPipelineRenderingCreateInfoKHR renderingCreateInfo{};
				renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
				renderingCreateInfo.colorAttachmentCount = 1;
				renderingCreateInfo.pColorAttachmentFormats = &imageFormat;
				renderingCreateInfo.depthAttachmentFormat = QueryDepthFormat();

				VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
				depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
				depthStencilInfo.depthTestEnable = enableDepthTesting;
				depthStencilInfo.depthWriteEnable = enableDepthTesting;
				depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
				depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
				depthStencilInfo.minDepthBounds = 0.0f; // Optional
				depthStencilInfo.maxDepthBounds = 1.0f; // Optional
				depthStencilInfo.stencilTestEnable = VK_FALSE;
				depthStencilInfo.front = {}; // Optional
				depthStencilInfo.back = {}; // Optional

				///////////////////////////////////////////////////////////////////////////////////////////////////////
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				std::vector<VkPipelineShaderStageCreateInfo> shaderPipelineCreateInfo;
				std::vector<VkShaderModule> shaderModules;
				auto extensions = vkdevice.GetDeviceExtensions();
				if (std::find_if(extensions.begin(), extensions.end(), [](std::string A) { return A.compare(VK_KHR_MAINTENANCE_5_EXTENSION_NAME) == 0; }) != extensions.end()) {
					for (size_t i = 0; i < shaders.size(); i++) {
						VkShaderModuleCreateInfo shaderCreateInfo{};
						shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
						auto shaderCode = ReadFile(std::get<1>(shaders[i]));
						shaderCreateInfo.codeSize = shaderCode.size();
						shaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

						VkPipelineShaderStageCreateInfo shaderStagePipelineInfo{};
						shaderStagePipelineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
						shaderStagePipelineInfo.stage = std::get<0>(shaders[i]);
						shaderStagePipelineInfo.pName = "main";
						shaderStagePipelineInfo.pNext = &shaderCreateInfo;
						shaderPipelineCreateInfo.push_back(shaderStagePipelineInfo);

						#if TVK_VALIDATION_LAYERS
						std::cout << "TinyVulkan: Loading Shader @ " << std::get<1>(shaders[i]) << std::endl;
						#endif
					}
				} else {
					for (size_t i = 0; i < shaders.size(); i++) {
						auto shaderCode = ReadFile(std::get<1>(shaders[i]));
						auto shaderModule = CreateShaderModule(shaderCode);
						shaderModules.push_back(shaderModule);
						shaderPipelineCreateInfo.push_back(CreateShaderInfo(std::get<1>(shaders[i]), shaderModule, std::get<0>(shaders[i])));
					}
				}
				
				VkGraphicsPipelineCreateInfo pipelineInfo{};
				pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
				pipelineInfo.stageCount = static_cast<uint32_t>(shaderPipelineCreateInfo.size());
				pipelineInfo.pStages = shaderPipelineCreateInfo.data();
				pipelineInfo.pVertexInputState = &vertexInputInfo;
				pipelineInfo.pInputAssemblyState = &inputAssembly;
				pipelineInfo.pViewportState = &viewportState;
				pipelineInfo.pRasterizationState = &rasterizer;
				pipelineInfo.pMultisampleState = &multisampling;
				pipelineInfo.pColorBlendState = &colorBlending;
				pipelineInfo.pDepthStencilState = &depthStencilInfo;

				pipelineInfo.pDynamicState = &dynamicState;
				pipelineInfo.pNext = &renderingCreateInfo;

				pipelineInfo.layout = pipelineLayout;
				pipelineInfo.renderPass = VK_NULL_HANDLE;
				pipelineInfo.subpass = 0;
				pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
				pipelineInfo.basePipelineIndex = -1; // Optional

				if (vkCreateGraphicsPipelines(vkdevice.GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, VK_NULL_HANDLE, &graphicsPipeline) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to create graphics pipeline!");
				
				for(auto shaderModule : shaderModules)
					vkDestroyShaderModule(vkdevice.GetLogicalDevice(), shaderModule, VK_NULL_HANDLE);
			}
			
		public:
			TinyVkVulkanDevice& vkdevice;

			TinyVkGraphicsPipeline operator=(const TinyVkGraphicsPipeline& pipeline) = delete;

			~TinyVkGraphicsPipeline() { this->Dispose(); }

			void Disposable(bool waitIdle) {
				if (waitIdle) vkdevice.DeviceWaitIdle();

				vkDestroyDescriptorSetLayout(vkdevice.GetLogicalDevice(), descriptorLayout, VK_NULL_HANDLE);
				vkDestroyPipeline(vkdevice.GetLogicalDevice(), graphicsPipeline, VK_NULL_HANDLE);
				vkDestroyPipelineLayout(vkdevice.GetLogicalDevice(), pipelineLayout, VK_NULL_HANDLE);
			}

			TinyVkGraphicsPipeline(TinyVkVulkanDevice& vkdevice, TinyVkVertexDescription vertexDescription, const std::vector<std::tuple<VkShaderStageFlagBits, std::string>> shaders, const std::vector<VkDescriptorSetLayoutBinding>& descriptorBindings, const std::vector<VkPushConstantRange>& pushConstantRanges, bool enableDepthTesting, VkFormat imageFormat = VK_FORMAT_B8G8R8A8_UNORM, VkColorComponentFlags colorComponentFlags = VKCOMP_RGBA, VkPipelineColorBlendAttachmentState colorBlendState = GetBlendDescription(true), VkPrimitiveTopology vertexTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VkPolygonMode polgyonTopology = VK_POLYGON_MODE_FILL)
			: vkdevice(vkdevice), imageFormat(imageFormat), vertexDescription(vertexDescription), descriptorBindings(descriptorBindings), pushConstantRanges(pushConstantRanges), colorComponentFlags(colorComponentFlags), colorBlendState(colorBlendState), vertexTopology(vertexTopology), polgyonTopology(polgyonTopology) {
				onDispose.hook(TinyVkCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));

				this->enableBlending = colorBlendState.blendEnable;
				this->enableDepthTesting = enableDepthTesting;

				TinyVkQueueFamily indices = vkdevice.FindQueueFamilies();
				vkGetDeviceQueue(vkdevice.GetLogicalDevice(), indices.graphicsFamily.value(), 0, &graphicsQueue);
				
				if (vkdevice.GetPresentSurface() != VK_NULL_HANDLE)
					vkGetDeviceQueue(vkdevice.GetLogicalDevice(), indices.presentFamily.value(), 0, &presentQueue);

				CreateGraphicsPipeline(shaders);
			}

			#pragma region REFERENCE_GETTERS

			VkPipelineLayout GetPipelineLayout() { return pipelineLayout; }
			VkPipeline GetGraphicsPipeline() { return graphicsPipeline; }
			VkQueue GetGraphicsQueue() { return graphicsQueue; }
			VkQueue GetPresentQueue() { return presentQueue; }
			
			#pragma endregion

			/// <summary>Returns the optimal VkFormat for the desired depth image format.</summary>
			VkFormat QueryDepthFormat(VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL) {
				const std::vector<VkFormat>& candidates = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
				for (VkFormat format : candidates) {
					VkFormatProperties props;
					vkGetPhysicalDeviceFormatProperties(vkdevice.GetPhysicalDevice(), format, &props);

					VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
					if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
						return format;
					} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
						return format;
					}
				}

				throw std::runtime_error("TinyVulkan: Failed to find supported format!");
			}

			/// <summary>Returns true/false whether alpha blending is enabled on the graphics pipeline.</summary>
			bool BlendingIsEnabled() { return enableBlending; }

			/// <summary>Returns true/false whether depthj fragment testing is enabled on the graphics pipeline.</summary>
			bool DepthTestingIsEnabled() { return enableDepthTesting; }

			/// <summary>Gets a generic Normal Blending Mode for creating a GraphicsPipeline with.</summary>
			inline static const VkPipelineColorBlendAttachmentState GetBlendDescription(bool isBlendingEnabled = true) {
				VkPipelineColorBlendAttachmentState colorBlendAttachment{};
				colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
				colorBlendAttachment.blendEnable = isBlendingEnabled;

				colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;

				colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
				return colorBlendAttachment;
			}

			/// <summary>Returns the push constant range info of a given size applied to the given shader stages.</summary>
			inline static VkPushConstantRange SelectPushConstantRange(uint32_t pushConstantRangeSize = 0, VkShaderStageFlags shaderStages = VK_SHADER_STAGE_ALL_GRAPHICS) {
				VkPushConstantRange pushConstantRange{};
				pushConstantRange.stageFlags = shaderStages;
				pushConstantRange.offset = 0;
				pushConstantRange.size = pushConstantRangeSize;
				return pushConstantRange;
			}

			/// <summary>Creates a layout description for how a descriptor should be bound to the graphics pipeline at hwta binding and shader stages./summary>
			inline static VkDescriptorSetLayoutBinding SelectPushDescriptorLayoutBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t descriptorCount = 1) {
				VkDescriptorSetLayoutBinding descriptorLayoutBinding {};
				descriptorLayoutBinding.binding = binding;
				descriptorLayoutBinding.descriptorCount = descriptorCount;
				descriptorLayoutBinding.descriptorType = descriptorType;
				descriptorLayoutBinding.pImmutableSamplers = VK_NULL_HANDLE;
				descriptorLayoutBinding.stageFlags = stageFlags;
				return descriptorLayoutBinding;
			}

			/// <summary>Creates a generic write descriptor to represent data passed to the GPU when rendering (on myrenderer.PushDescriptorSet).</summary>
			inline static VkWriteDescriptorSet SelectWriteDescriptor(uint32_t binding, uint32_t descriptorCount, VkDescriptorType descriptorType, const VkDescriptorImageInfo* imageInfo, const VkDescriptorBufferInfo* bufferInfo) {
				VkWriteDescriptorSet writeDescriptorSets{};
				writeDescriptorSets.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSets.dstSet = 0;
				writeDescriptorSets.dstBinding = binding;
				writeDescriptorSets.descriptorCount = descriptorCount;
				writeDescriptorSets.descriptorType = descriptorType;
				writeDescriptorSets.pImageInfo = imageInfo;
				writeDescriptorSets.pBufferInfo = bufferInfo;
				return writeDescriptorSets;
			}

			/// <summary>Creates a write image descriptor (Combined Image Sampler) for passing images to the GPU (on myrenderer.PushDescriptorSet).</summary>
			inline static VkWriteDescriptorSet SelectWriteImageDescriptor(uint32_t binding, uint32_t descriptorCount, const VkDescriptorImageInfo* imageInfo) {
				VkWriteDescriptorSet writeDescriptorSets{};
				writeDescriptorSets.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSets.dstSet = 0;
				writeDescriptorSets.dstBinding = binding;
				writeDescriptorSets.descriptorCount = descriptorCount;
				writeDescriptorSets.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				writeDescriptorSets.pImageInfo = imageInfo;
				return writeDescriptorSets;
			}

			/// <summary>Creates a write buffer descriptor (any of VK_DESCRIPTOR_TYPE_*_BUFFER) for passing buffers to the GPU (on myrenderer.PushDescriptorSet).</summary>
			inline static VkWriteDescriptorSet SelectWriteBufferDescriptor(uint32_t binding, uint32_t descriptorCount, const VkDescriptorBufferInfo* bufferInfo) {
				VkWriteDescriptorSet writeDescriptorSets{};
				writeDescriptorSets.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSets.dstSet = 0;
				writeDescriptorSets.dstBinding = binding;
				writeDescriptorSets.descriptorCount = descriptorCount;
				writeDescriptorSets.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				writeDescriptorSets.pBufferInfo = bufferInfo;
				return writeDescriptorSets;
			}
		
			/// <summary>Creates a layout description for how a descriptor should be bound to the graphics pipeline at hwta binding and shader stages./summary>
			inline static VkDescriptorSetLayoutBinding SelectPushDescriptorLayoutBinding(uint32_t binding, TinyVkDescriptorTypes descriptorType, VkShaderStageFlags stageFlags, uint32_t descriptorCount = 1) {
				VkDescriptorSetLayoutBinding descriptorLayoutBinding {};
				descriptorLayoutBinding.binding = binding;
				descriptorLayoutBinding.descriptorCount = descriptorCount;
				descriptorLayoutBinding.descriptorType = static_cast<VkDescriptorType>(descriptorType);
				descriptorLayoutBinding.pImmutableSamplers = VK_NULL_HANDLE;
				descriptorLayoutBinding.stageFlags = stageFlags;
				return descriptorLayoutBinding;
			}

			/// <summary>Creates a generic write descriptor to represent data passed to the GPU when rendering (on myrenderer.PushDescriptorSet).</summary>
			inline static VkWriteDescriptorSet SelectWriteDescriptor(uint32_t binding, uint32_t descriptorCount, TinyVkDescriptorTypes descriptorType, const VkDescriptorImageInfo* imageInfo, const VkDescriptorBufferInfo* bufferInfo) {
				VkWriteDescriptorSet writeDescriptorSets{};
				writeDescriptorSets.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSets.dstSet = 0;
				writeDescriptorSets.dstBinding = binding;
				writeDescriptorSets.descriptorCount = descriptorCount;
				writeDescriptorSets.descriptorType = static_cast<VkDescriptorType>(descriptorType);
				writeDescriptorSets.pImageInfo = imageInfo;
				writeDescriptorSets.pBufferInfo = bufferInfo;
				return writeDescriptorSets;
			}
		};
	}
#endif