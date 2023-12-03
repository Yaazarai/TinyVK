#pragma once
#ifndef TINYVK_TINYVKCOMMANDPOOL
#define TINYVK_TINYVKCOMMANDPOOL
	#include "./TinyVulkan.hpp"

	namespace TINYVULKAN_NAMESPACE {
		/// <summary>Pool of managed rentable VkCommandBuffers for performing rendering/transfer operations.</summary>
		class TinyVkCommandPool : public TinyVkDisposable {
		private:
			VkCommandPool commandPool;
			size_t bufferCount;

			void CreateCommandPool() {
				VkCommandPoolCreateInfo poolInfo{};
				poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
				poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
				poolInfo.queueFamilyIndex = vkdevice.FindQueueFamilies().graphicsFamily.value();

				if (vkCreateCommandPool(vkdevice.GetLogicalDevice(), &poolInfo, VK_NULL_HANDLE, &commandPool) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to create command pool!");
			}

			void CreateCommandBuffers(size_t bufferCount = 1) {
				commandBuffers.resize(bufferCount);
				rentQueue.resize(bufferCount);

				for (int32_t i = 0; i < bufferCount; i++) {
					commandBuffers[i] = VK_NULL_HANDLE;
					rentQueue[i] = false;
				}

				VkCommandBufferAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				allocInfo.commandPool = commandPool;
				allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

				if (vkAllocateCommandBuffers(vkdevice.GetLogicalDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to allocate command buffers!");
			}

		public:
			TinyVkVulkanDevice& vkdevice;
			std::vector<VkCommandBuffer> commandBuffers;
			std::vector<bool> rentQueue;

			TinyVkCommandPool operator=(const TinyVkCommandPool& cmdPool) = delete;

			~TinyVkCommandPool() { this->Dispose(); }

			void Disposable(bool waitIdle) {
				if (waitIdle) vkDeviceWaitIdle(vkdevice.GetLogicalDevice());

				vkDestroyCommandPool(vkdevice.GetLogicalDevice(), commandPool, VK_NULL_HANDLE);
			}
			
			/// <summary>Creates a command pool to lease VkCommandBuffers from for recording render commands.</summary>
			TinyVkCommandPool(TinyVkVulkanDevice& vkdevice, size_t bufferCount = 32ULL) : vkdevice(vkdevice), bufferCount(bufferCount) {
				onDispose.hook(TinyVkCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));

				CreateCommandPool();
				CreateCommandBuffers(bufferCount+1);
			}

			/// <summary>Returns the underlying VkCommandPool.</summary>
			VkCommandPool& GetPool() { return commandPool; }
			
			/// <summary>Returns the underling list of VkCommandBuffers.</summary>
			std::vector<VkCommandBuffer>& GetBuffers() { return commandBuffers; }
			
			/// <summary>Returns the total number of allocated VkCommandBuffers.</summary>
			size_t GetBufferCount() { return commandBuffers.size(); }

			/// <summary>Returns true/false if ANY VkCommandBuffers are available to be Lealsed.</summary>
			bool HasBuffers() {
				for (int32_t i = 0; i < rentQueue.size(); i++)
					if (rentQueue[i] == false) return true;

				return false;
			}

			/// <summary>Returns the number of available VkCommandBuffers that can be Leased.</summary>
			int32_t HasBuffersCount() {
				int32_t hasSum = 0;

				for (int32_t i = 0; i < rentQueue.size(); i++)
					hasSum += (rentQueue[i] == false)?1:0;

				return hasSum;
			}

			/// <summary>Reserves a VkCommandBuffer for use and returns the VkCommandBuffer and it's ID (used for returning to the pool).</summary>
			std::pair<VkCommandBuffer,int32_t> LeaseBuffer(bool resetCmdBuffer = false) {
				for (int32_t i = 0; i < rentQueue.size(); i++)
					if (rentQueue[i] == false) {
						rentQueue[i] = true;
						
						if (resetCmdBuffer)
							vkResetCommandBuffer(commandBuffers[i], 0);

						return std::pair(commandBuffers[i], i);
					}

				throw std::runtime_error("TinyVulkan: VKCommandPool is full and cannot lease any more VkCommandBuffers! MaxSize: " + std::to_string(bufferCount));
				return std::pair<VkCommandBuffer,int32_t>(VK_NULL_HANDLE,-1);
			}

			/// <summary>Free's up the VkCommandBuffer that was previously rented for re-use.</summary>
			void ReturnBuffer(std::pair<VkCommandBuffer, int32_t> bufferIndexPair) {
				if (bufferIndexPair.second < 0 || bufferIndexPair .second >= rentQueue.size())
					throw std::runtime_error("TinyVulkan: Failed to return command buffer!");

				rentQueue[bufferIndexPair.second] = false;
			}

			/// <summary>Sets all of the command buffers to available--optionally resets their recorded commands.</summary>
			void ReturnAllBuffers(bool resetCmdPool = false) {
				if (resetCmdPool) vkResetCommandPool(vkdevice.GetLogicalDevice(), commandPool, 0);

				for(size_t i = 0; i < rentQueue.size(); i++)
					rentQueue[i] = false;
			}
		};
	}
#endif