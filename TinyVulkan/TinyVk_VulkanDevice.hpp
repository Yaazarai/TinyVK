#pragma once
#ifndef TINYVK_TINYVKVULKANDEVICE
#define TINYVK_TINYVKVULKANDEVICE

	#include "./TinyVulkan.hpp"
	
	namespace TINYVULKAN_NAMESPACE {
		#define VK_VALIDATION_LAYER_KHRONOS_EXTENSION_NAME "VK_LAYER_KHRONOS_validation"

		struct TinyVkQueueFamily {
			uint32_t graphicsFamily, presentFamily, computeFamily;
			bool hasGraphicsFamily, hasPresentFamily, hasComputeFamily;

			TinyVkQueueFamily() : graphicsFamily(0), presentFamily(0), computeFamily(0), hasGraphicsFamily(false), hasPresentFamily(false), hasComputeFamily(false) {}
			void SetGraphicsFamily(uint32_t queueFamily) { graphicsFamily = queueFamily; hasGraphicsFamily = true; }
			void SetPresentFamily(uint32_t queueFamily) { presentFamily = queueFamily; hasPresentFamily = true; }
			void SetComputeFamily(uint32_t queueFamily) { computeFamily = queueFamily; hasComputeFamily = true; }
			bool HasGraphicsFamily() { return hasGraphicsFamily; }
			bool HasPresentFamily() { return hasPresentFamily; }
			bool HasComputeFamily() { return hasComputeFamily; }
		};

		union VkPhysicalDeviceFeaturesUnionArray {
			VkBool32 features[sizeof(VkPhysicalDeviceFeatures)/sizeof(VkBool32)];
			VkPhysicalDeviceFeatures vkfeatures;
		};

		/// @brief Vulkan Instance & Render(Physical/Logical) Device & VMAllocator Loader.
		class TinyVkVulkanDevice : public TinyVkDisposable {
		private:
			std::vector<const char*> validationLayers = { VK_VALIDATION_LAYER_KHRONOS_EXTENSION_NAME };
			std::vector<const char*> deviceExtensions = { VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME };
			std::vector<const char*> instanceExtensions = {  };

			const std::vector<VkPhysicalDeviceType> deviceTypes;
			VkPhysicalDeviceFeatures deviceFeatures {};
			const bool useComputeBit;

			VkApplicationInfo appInfo{};
			VkInstance instance = VK_NULL_HANDLE;
			VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

			VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
			VkDevice logicalDevice = VK_NULL_HANDLE;
			VkSurfaceKHR presentSurface = VK_NULL_HANDLE;
			VmaAllocator memoryAllocator = VK_NULL_HANDLE;

			/// @brief Creates the underlying Vulkan Instance w/ Required Extensions.
			void CreateVkInstance(const std::string& title, TinyVkWindow* window = VK_NULL_HANDLE) {
				VkApplicationInfo appInfo {};
				appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
				appInfo.pApplicationName = title.c_str();
				appInfo.applicationVersion = TVK_RENDERER_VERSION;
				appInfo.pEngineName = TVK_RENDERER_NAME;
				appInfo.engineVersion = TVK_RENDERER_VERSION;
				appInfo.apiVersion = TVK_RENDERER_VERSION;

				VkInstanceCreateInfo createInfo {};
				createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
				createInfo.pApplicationInfo = &appInfo;

				#if TVK_VALIDATION_LAYERS
					if (!QueryValidationLayerSupport()) {
						throw TinyVkRuntimeError("TinyVulkan: Failed to initialize validation layers!");
					} else {
						std::cout << "TinyVulkan: Enabled Validation Layers:" << std::endl;
						for(const char* layer : validationLayers)
							std::cout << "\t" << layer << std::endl;
					}

					VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo {};
					debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
					debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
					debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
					debugCreateInfo.pfnUserCallback = DebugCallback;
					debugCreateInfo.pUserData = VK_NULL_HANDLE;

					createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
					createInfo.ppEnabledLayerNames = validationLayers.data();
					createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
					
					instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
				#endif

				if (window != VK_NULL_HANDLE)
					for (const auto& extension : window->QueryRequiredExtensions())
						instanceExtensions.push_back(extension);
				
				createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
				createInfo.ppEnabledExtensionNames = instanceExtensions.data();

				VkResult result = vkCreateInstance(&createInfo, VK_NULL_HANDLE, &instance);
				if (result != VK_SUCCESS)
					throw TinyVkRuntimeError("TinyVulkan: Failed to create Vulkan instance! " + result);

				#if TVK_VALIDATION_LAYERS
					result = CreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, VK_NULL_HANDLE, &debugMessenger);
					if (result != VK_SUCCESS)
						throw TinyVkRuntimeError("TinyVulkan: Failed to set up debug messenger! " + result);

					std::cout << "TinyVulkan: " << instanceExtensions.size() << " instance extensions supported." << std::endl;
					for (const auto& extension : instanceExtensions) std::cout << '\t' << extension << std::endl;
				#endif
			}
			
			/// @brief Returns an VkDeviceSize ranking of VK_PHYSICAL_DEVICE_TYPE for a VkPhysicalDevice ranked by memory heap size.
			VkDeviceSize QueryPhysicalDeviceRank(VkPhysicalDevice device) {
				VkPhysicalDeviceProperties2 properties {};
				properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
				vkGetPhysicalDeviceProperties2(device, &properties);

				VkDeviceSize rank = 0;
				switch(properties.properties.deviceType) {
					case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: rank = 400; break;
					case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: rank = 300; break;
					case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: rank = 200; break;
					case VK_PHYSICAL_DEVICE_TYPE_CPU: rank = 100; break;
					case VK_PHYSICAL_DEVICE_TYPE_OTHER: rank = 0; break;
				}

				VkPhysicalDeviceMemoryProperties2 memoryProperties {};
				memoryProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
				vkGetPhysicalDeviceMemoryProperties2(device, &memoryProperties);\

				for(VkMemoryHeap heap : memoryProperties.memoryProperties.memoryHeaps)
					if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
						return rank + (heap.size / 1000 / 1000 / 1000);

				return 0;
			}

			/// @brief Returns the first usable GPU.
			void QueryPhysicalDevice() {
				std::vector<VkPhysicalDevice> suitableDevices = QuerySuitableDevices();
				std::sort(suitableDevices.begin(), suitableDevices.end(), [this](VkPhysicalDevice A, VkPhysicalDevice B) {
					return QueryPhysicalDeviceRank(A) >= QueryPhysicalDeviceRank(B);
				});
				physicalDevice = (suitableDevices.size() > 0)? suitableDevices.front() : VK_NULL_HANDLE;

				if (physicalDevice == VK_NULL_HANDLE)
					throw TinyVkRuntimeError("TinyVulkan: Failed to find a suitable GPU!");
				
				#if TVK_VALIDATION_LAYERS
					VkPhysicalDevicePushDescriptorPropertiesKHR pushDescriptorProps {};
					pushDescriptorProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR;

					VkPhysicalDeviceProperties2 deviceProperties {};
					deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
					deviceProperties.pNext = &pushDescriptorProps;

					vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties);

					std::cout << "TinyVulkan: GPU Hardware Info" << std::endl;
					std::cout << "\tGPU Device Name:        " << deviceProperties.properties.deviceName << std::endl;
					std::cout << "\tDevice Rank:            " << QueryPhysicalDeviceRank(physicalDevice) << std::endl;
					std::cout << "\tPush Constant Memory:   " << deviceProperties.properties.limits.maxPushConstantsSize << " Bytes" << std::endl;
					std::cout << "\tPush Descriptor Memory: " << pushDescriptorProps.maxPushDescriptors << " Count" << std::endl;
					
					TinyVkQueueFamily indices = FindQueueFamilies(physicalDevice);
					std::cout << "\tGraphics Pipeline:       " << (indices.hasGraphicsFamily?"true (enabled)":"false (enabled)") << " / " << (indices.hasComputeFamily?"true (compatible)":"false (compatible)") << std::endl;
					std::cout << "\tPresent Pipeline:       " << (indices.hasPresentFamily?"true (enabled)":"false (enabled)") << " / " << (indices.hasComputeFamily?"true (compatible)":"false (compatible)") << std::endl;
					std::cout << "\tCompute Pipeline:       " << (indices.hasComputeFamily?"true (enabled)":"false (enabled)") << " / " << (indices.hasComputeFamily?"true (compatible)":"false (compatible)") << std::endl;
				#endif
			}
			
			/// @brief Creates the logical devices for the graphics/present queue families.
			void CreateLogicalDevice() {
				TinyVkQueueFamily indices = FindQueueFamilies();
				std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
				
				uint32_t graphicsFamily, presentFamily, computeFamily;
				graphicsFamily = indices.hasGraphicsFamily? indices.graphicsFamily : 0;
				presentFamily = indices.hasPresentFamily? indices.presentFamily : graphicsFamily;
				computeFamily = indices.hasComputeFamily? indices.computeFamily : graphicsFamily;
				std::set<uint32_t> uniqueQueueFamilies = { graphicsFamily, presentFamily, computeFamily };

				float queuePriority = 1.0f;
				for (uint32_t queueFamily : uniqueQueueFamilies) {
					VkDeviceQueueCreateInfo queueCreateInfo{};
					queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
					queueCreateInfo.queueFamilyIndex = queueFamily;
					queueCreateInfo.queueCount = 1;
					queueCreateInfo.pQueuePriorities = &queuePriority;
					queueCreateInfos.push_back(queueCreateInfo);
				}

				VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingCreateInfo{};
				dynamicRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
				dynamicRenderingCreateInfo.dynamicRendering = VK_TRUE;

				VkDeviceCreateInfo createInfo{};
				createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
				createInfo.pNext = &dynamicRenderingCreateInfo;
				createInfo.pQueueCreateInfos = queueCreateInfos.data();
				createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
				createInfo.pEnabledFeatures = &deviceFeatures;

				if (indices.hasPresentFamily)
					deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
				
				createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
				createInfo.ppEnabledExtensionNames = deviceExtensions.data();

				#if TVK_VALIDATION_LAYERS
					createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
					createInfo.ppEnabledLayerNames = validationLayers.data();
				#else
					createInfo.enabledLayerCount = 0;
					createInfo.ppEnabledLayerNames = VK_NULL_HANDLE;
				#endif

				if (vkCreateDevice(physicalDevice, &createInfo, VK_NULL_HANDLE, &logicalDevice) != VK_SUCCESS)
					throw TinyVkRuntimeError("TinyVulkan: Failed to create logical device! Missing extension or queue family!");

				#if TVK_VALIDATION_LAYERS
					std::cout << "TinyVulkan: " << deviceExtensions.size() << " device extensions supported." << std::endl;
					for (const auto& extension : deviceExtensions) std::cout << '\t' << extension << std::endl;
				#endif
			}
			
			/// @brief Creates the VMAllocator for AMD's GPU memory handling API.
			void CreateVMAllocator() {
				VmaAllocatorCreateInfo allocatorCreateInfo {};
				allocatorCreateInfo.vulkanApiVersion = TVK_RENDERER_VERSION;
				allocatorCreateInfo.physicalDevice = physicalDevice;
				allocatorCreateInfo.device = logicalDevice;
				allocatorCreateInfo.instance = instance;
				vmaCreateAllocator(&allocatorCreateInfo, &memoryAllocator);
			}
			
		public:

			TinyVkVulkanDevice operator=(const TinyVkVulkanDevice&) = delete;

			~TinyVkVulkanDevice() { this->Dispose(); }

			void Disposable(bool waitIdle) {
				if (waitIdle) DeviceWaitIdle();

				#if TVK_VALIDATION_LAYERS
					DestroyDebugUtilsMessengerEXT(instance, debugMessenger, VK_NULL_HANDLE);
				#endif
				
				vmaDestroyAllocator(memoryAllocator);
				vkDestroyDevice(logicalDevice, VK_NULL_HANDLE);
				if (presentSurface != VK_NULL_HANDLE)
					vkDestroySurfaceKHR(instance, presentSurface, VK_NULL_HANDLE);
				vkDestroyInstance(instance, VK_NULL_HANDLE);
			}

			TinyVkVulkanDevice(const std::string title, bool useComputeBit = false, const std::vector<VkPhysicalDeviceType> deviceTypes = { VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU }, TinyVkWindow* window = VK_NULL_HANDLE, VkPhysicalDeviceFeatures deviceFeatures = { .multiDrawIndirect = VK_TRUE })
			: useComputeBit(useComputeBit), deviceTypes(deviceTypes) {
				onDispose.hook(TinyVkCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
				
				VkPhysicalDeviceFeaturesUnionArray featuresA = { .vkfeatures = this->deviceFeatures }, featuresB = { .vkfeatures = deviceFeatures };
				for(size_t i = 0; i < sizeof(VkPhysicalDeviceFeatures)/sizeof(VkBool32); i++) featuresA.features[i] |= featuresB.features[i];
				this->deviceFeatures = featuresA.vkfeatures;

				CreateVkInstance(title, window);

				if (window != VK_NULL_HANDLE)
					presentSurface = window->CreateWindowSurface(instance);

				vkCmdRenderingGetCallbacks(instance);
				QueryPhysicalDevice();
				CreateLogicalDevice();
				CreateVMAllocator();
			}

			#pragma region REFERENCE_GETTERS

			VkInstance GetInstance() { return instance; }
			VkDebugUtilsMessengerEXT GetDebugMessenger() { return debugMessenger; }
			VkPhysicalDevice GetPhysicalDevice() { return physicalDevice; }
			VkDevice GetLogicalDevice() { return logicalDevice; }
			VkSurfaceKHR GetPresentSurface() { return presentSurface; }
			VmaAllocator GetAllocator() { return memoryAllocator; }
			VkApplicationInfo GetAppInfo() { return appInfo; }
			const std::vector<const char*> GetDeviceExtensions() { return deviceExtensions; }
			const bool IsComputeCompatible() { return useComputeBit; }

			#pragma endregion
			#pragma region VULKAN_VALIDATION_LAYERS
			
			/// @brief Queries device drivers for installed Validation Layers.
			bool QueryValidationLayerSupport() {
				uint32_t layersFound = 0, layersCount = 0;
				vkEnumerateInstanceLayerProperties(&layersCount, VK_NULL_HANDLE);
				std::vector<VkLayerProperties> availableLayers(layersCount);
				vkEnumerateInstanceLayerProperties(&layersCount, availableLayers.data());

				#if TVK_VALIDATION_LAYERS
					std::cout << "TinyVulkan: Available Validation Layers:" << std::endl;
				#endif
				for (const auto& layerProperties : availableLayers) {
					#if TVK_VALIDATION_LAYERS
					std::cout << "\t" << layerProperties.layerName << std::endl;
					#endif

					for (const std::string layerName : validationLayers)
						if (!layerName.compare(layerProperties.layerName)) layersFound++;
				}

				return (layersFound == validationLayers.size());
			}

			#pragma endregion
			#pragma region VULKAN_GPU_DEVICE

			/// @brief Wait for GPU device to finish transfer/render commands.
			inline VkResult DeviceWaitIdle() { return vkDeviceWaitIdle(logicalDevice); }

			/// @brief Returns info about the VkPhysicalDevice graphics/present queue families. If no surface provided, auto checks for Win32 surface support.
			TinyVkQueueFamily FindQueueFamilies(VkPhysicalDevice newDevice = VK_NULL_HANDLE) {
				VkPhysicalDevice device = (newDevice == VK_NULL_HANDLE)? physicalDevice : newDevice;

				uint32_t queueFamilyCount = 0;
				vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, VK_NULL_HANDLE);
				std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
				vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
				
				TinyVkQueueFamily indices = {};
				VkBool32 presentSupport = false;
				for (int i = 0; i < queueFamilies.size(); i++) {
					if (presentSurface != VK_NULL_HANDLE)
							vkGetPhysicalDeviceSurfaceSupportKHR(device, i, presentSurface, &presentSupport);
					
					if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { indices.SetGraphicsFamily(i); }
					if (presentSupport) { indices.SetPresentFamily(i); }
					if (useComputeBit && queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { indices.SetComputeFamily(i); }
					if (indices.hasGraphicsFamily
						&& (!presentSupport || (presentSupport && indices.hasPresentFamily))
						&& (!useComputeBit || (useComputeBit && indices.hasComputeFamily)))
							break;
				}
				return indices;
			}

			/// @brief Checks the VkPhysicalDevice for swap-chain availability.
			TinyVkSwapChainSupporter QuerySwapChainSupport(VkPhysicalDevice device) {
				TinyVkSwapChainSupporter details;

				uint32_t formatCount = 0;
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, presentSurface, &formatCount, VK_NULL_HANDLE);
				details.formats.resize(formatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, presentSurface, &formatCount, details.formats.data());
				vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, presentSurface, &details.capabilities);

				uint32_t presentModeCount = 0;
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, presentSurface, &presentModeCount, VK_NULL_HANDLE);
				details.presentModes.resize(presentModeCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, presentSurface, &presentModeCount, details.presentModes.data());
				return details;
			}

			/// @brief Returns BOOL(true/false) if the VkPhysicalDevice (GPU/iGPU) supports extensions.
			bool QueryDeviceExtensionSupport(VkPhysicalDevice device) {
				uint32_t extensionCount;
				vkEnumerateDeviceExtensionProperties(device, VK_NULL_HANDLE, &extensionCount, VK_NULL_HANDLE);
				std::vector<VkExtensionProperties> availableExtensions(extensionCount);
				vkEnumerateDeviceExtensionProperties(device, VK_NULL_HANDLE, &extensionCount, availableExtensions.data());
				
				std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
				for (const auto& extension : availableExtensions)
					requiredExtensions.erase(extension.extensionName);

				#if TVK_VALIDATION_LAYERS
					if (requiredExtensions.size() > 0)
						std::cout << "TinyVulkan: Unavailable Extensions: " << requiredExtensions.size() << std::endl;
					
					for (const auto& extension : requiredExtensions) std::cout << "\t" << extension << std::endl;
				#endif

				return requiredExtensions.empty();
			}

			/// @brief Returns BOOL(true/false) if a physical device is compatible with the required vulkan renderer (with or wihtout present support).
			bool QueryDeviceCompatibility(VkPhysicalDevice device) {
				VkPhysicalDeviceProperties2 deviceProperties {};
				deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
				vkGetPhysicalDeviceProperties2(device, &deviceProperties);
				
				VkPhysicalDeviceFeatures deviceFeatures {};
				vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
				
				TinyVkQueueFamily indices = FindQueueFamilies(device);
				bool supportsExtensions = QueryDeviceExtensionSupport(device);

				bool swapChainAdequate = false;
				if (presentSurface != VK_NULL_HANDLE) {
					TinyVkSwapChainSupporter swapChainSupport = QuerySwapChainSupport(device);
					swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
				}
				bool requiresPresent = (presentSurface == VK_NULL_HANDLE) || (presentSurface != VK_NULL_HANDLE && indices.hasPresentFamily && swapChainAdequate);
				
				bool hasType = false;
				for (auto type : deviceTypes)
					hasType = (!hasType)? (deviceProperties.properties.deviceType == type) : hasType;
				
				bool hasFeatures = true;
				VkPhysicalDeviceFeaturesUnionArray featuresA = { .vkfeatures = this->deviceFeatures }, featuresB = { .vkfeatures = deviceFeatures };
				for(size_t i = 0; i < sizeof(VkPhysicalDeviceFeatures)/sizeof(VkBool32); i++)
					if (featuresA.features[i] && !featuresB.features[i]) hasFeatures = false;

				bool hasCompute = (useComputeBit && indices.hasComputeFamily) || !useComputeBit;
				return indices.hasGraphicsFamily && hasCompute && hasType && supportsExtensions && requiresPresent && hasFeatures;
			}

			/// @brief Returns a Vector of suitable VkPhysicalDevices (GPU/iGPU).
			std::vector<VkPhysicalDevice> QuerySuitableDevices() {
				uint32_t deviceCount = 0;
				vkEnumeratePhysicalDevices(instance, &deviceCount, VK_NULL_HANDLE);
				std::vector<VkPhysicalDevice> suitableDevices(deviceCount);
				vkEnumeratePhysicalDevices(instance, &deviceCount, suitableDevices.data());
				std::erase_if(suitableDevices, [this](VkPhysicalDevice device) { return !QueryDeviceCompatibility(device); });
				return suitableDevices;
			}

			#pragma endregion
		};
	}

#endif