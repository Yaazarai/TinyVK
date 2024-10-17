#pragma once
#ifndef TINYVK_TINYVKCOMMANDPOOL
#define TINYVK_TINYVKCOMMANDPOOL
	#include "./TinyVulkan.hpp"

	namespace TINYVULKAN_NAMESPACE {
		/// @brief Submission stage of the command buffer (for getting pipeline barrier info).
		enum class TinyVkCmdBufferSubmitStage {
			TINYVK_BEGIN,       /// Pre-Render & Pre-Pipeline-Access & Pipeline-Acess submit stage.
			TINYVK_END,         /// Post-Render & Post-Pipeline-Acess submit stage.
			TINYVK_BEGIN_TO_END /// No-Pipeline-Access submit stage (for layout transitions).
		};

		/// @brief Pool of managed rentable VkCommandBuffers for performing rendering/transfer operations.
		class TinyVkCommandPool : public TinyVkDisposable {
		private:
			VkCommandPool commandPool;
			size_t bufferCount;

			void CreateCommandPool() {
				VkCommandPoolCreateInfo poolInfo{};
				poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
				poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

				TinyVkQueueFamily queueFamily = vkdevice.FindQueueFamilies();
				
				if (useAsComputeCommandPool && queueFamily.HasComputeFamily()) {
					poolInfo.queueFamilyIndex = queueFamily.computeFamily;
				} else if (queueFamily.HasGraphicsFamily()) poolInfo.queueFamilyIndex = queueFamily.graphicsFamily;

				if (!queueFamily.HasGraphicsFamily() || (useAsComputeCommandPool && !queueFamily.HasComputeFamily()))
					throw TinyVkRuntimeError("TinyVulkan: Could not locate graphics or compute queue families for TinyVkCommandPool!");
				
				if (vkCreateCommandPool(vkdevice.GetLogicalDevice(), &poolInfo, VK_NULL_HANDLE, &commandPool) != VK_SUCCESS)
					throw TinyVkRuntimeError("TinyVulkan: Failed to create command pool!");
			}

			void CreateCommandBuffers(size_t bufferCount = 1) {
				VkCommandBufferAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				allocInfo.commandPool = commandPool;
				allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				allocInfo.commandBufferCount = static_cast<uint32_t>(bufferCount);

				std::vector<VkCommandBuffer> temporary(bufferCount);
				if (vkAllocateCommandBuffers(vkdevice.GetLogicalDevice(), &allocInfo, temporary.data()) != VK_SUCCESS)
					throw TinyVkRuntimeError("TinyVulkan: Failed to allocate command buffers!");

				auto& buffers = commandBuffers;
				std::for_each(temporary.begin(), temporary.end(), [&buffers](VkCommandBuffer cmdBuffer) {
					buffers.push_back(std::pair(cmdBuffer, static_cast<VkBool32>(false)));
				});
			}

		public:
			TinyVkVulkanDevice& vkdevice;
			std::vector<std::pair<VkCommandBuffer, VkBool32>> commandBuffers;
			static const size_t defaultCommandPoolSize = 32UL;
			const bool useAsComputeCommandPool;

			TinyVkCommandPool operator=(const TinyVkCommandPool& cmdPool) = delete;

			~TinyVkCommandPool() { this->Dispose(); }

			void Disposable(bool waitIdle) {
				if (waitIdle) vkdevice.DeviceWaitIdle();

				vkDestroyCommandPool(vkdevice.GetLogicalDevice(), commandPool, VK_NULL_HANDLE);
			}
			
			/// @brief Creates a command pool to lease VkCommandBuffers from for recording render commands.
			TinyVkCommandPool(TinyVkVulkanDevice& vkdevice, bool useAsComputeCommandPool, size_t bufferCount = defaultCommandPoolSize) : vkdevice(vkdevice), useAsComputeCommandPool(useAsComputeCommandPool), bufferCount(bufferCount) {
				onDispose.hook(TinyVkCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));

				CreateCommandPool();
				CreateCommandBuffers(bufferCount+1);
			}

			#pragma region REFERENCE_GETTERS

			VkCommandPool& GetPool() { return commandPool; }
			std::vector<std::pair<VkCommandBuffer, VkBool32>>& GetBuffers() { return commandBuffers; }
			size_t GetBufferCount() { return commandBuffers.size(); }
			static const size_t GetDefaultPoolSize() { return defaultCommandPoolSize; }

			#pragma endregion
			#pragma region COMMAND_POOL_HANDLING

			/// @brief Returns true/false if ANY VkCommandBuffers are available to be Leased.
			bool HasBuffers() {
				for (auto& cmdBuffer : commandBuffers)
					if (!cmdBuffer.second) return true;

				return false;
			}

			/// @brief Returns the number of available VkCommandBuffers that can be Leased.
			size_t HasBuffersCount() {
				size_t count = 0;
				for(auto& cmdBuffer : commandBuffers)
					count += static_cast<size_t>(!cmdBuffer.second);
				return count;
			}

			/// @brief Reserves a VkCommandBuffer for use and returns the VkCommandBuffer and it's ID (used for returning to the pool).
			std::pair<VkCommandBuffer,int32_t> LeaseBuffer(bool resetCmdBuffer = false) {
				size_t index = 0;
				for(auto& cmdBuffer : commandBuffers)
					if (!cmdBuffer.second) {
						cmdBuffer.second = true;
						if (resetCmdBuffer) vkResetCommandBuffer(cmdBuffer.first, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
						return std::pair(cmdBuffer.first, index++);
					}
				
				throw TinyVkRuntimeError("TinyVulkan: VKCommandPool is full and cannot lease any more VkCommandBuffers! MaxSize: " + std::to_string(bufferCount));
				return std::pair<VkCommandBuffer,int32_t>(VK_NULL_HANDLE,-1);
			}

			/// @brief Free's up the VkCommandBuffer that was previously rented for re-use.
			void ReturnBuffer(std::pair<VkCommandBuffer, int32_t> bufferIndexPair) {
				if (bufferIndexPair.second < 0 || bufferIndexPair .second >= commandBuffers.size())
					throw TinyVkRuntimeError("TinyVulkan: Failed to return command buffer!");

				commandBuffers[bufferIndexPair.second].second = false;
			}

			/// @brief Sets all of the command buffers to available--optionally resets their recorded commands.
			void ReturnAllBuffers() {
				vkResetCommandPool(vkdevice.GetLogicalDevice(), commandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
				
				for(auto& cmdBuffer : commandBuffers)
					cmdBuffer.second = false;
			}
			
			#pragma endregion
		};
	}
#endif