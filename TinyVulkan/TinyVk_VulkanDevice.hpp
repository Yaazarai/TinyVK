#pragma once
#ifndef TINYVK_TINYVKVULKANDEVICE
#define TINYVK_TINYVKVULKANDEVICE

	#include "./TinyVulkan.hpp"
	
	namespace TINYVULKAN_NAMESPACE {
		#define VK_VALIDATION_LAYER_KHRONOS_EXTENSION_NAME "VK_LAYER_KHRONOS_validation"

		struct TinyVkQueueFamily {
			std::optional<uint32_t> graphicsFamily;
			std::optional<uint32_t> presentFamily;

			/// <summary>Returns true/false if this is a complete graphics queue family.</summary>
			bool HasGraphicsFamily() { return graphicsFamily.has_value(); }
			
			/// <summary>Returns true/false if this is a complete present queue family.</summary>
			bool HasPresentFamily() { return presentFamily.has_value(); }
		};

		union VkPhysicalDeviceFeaturesUnionArray {
			VkBool32 features[sizeof(VkPhysicalDeviceFeatures)/sizeof(VkBool32)];
			VkPhysicalDeviceFeatures vkfeatures;
		};

		/// <summary>Vulkan Instance & Render(Physical/Logical) Device & VMAllocator Loader.</summary>
		class TinyVkVulkanDevice : public TinyVkDisposable {
		private:
			const std::vector<const char*> validationLayers = { VK_VALIDATION_LAYER_KHRONOS_EXTENSION_NAME };
			const std::vector<const char*> instanceExtensions = {  };
			const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME };
			const std::vector<VkPhysicalDeviceType> deviceTypes;
			VkPhysicalDeviceFeatures deviceFeatures = {};
			std::vector<const char*> presentExtensionNames;

			VkApplicationInfo appInfo{};
			VkInstance instance = VK_NULL_HANDLE;
			VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

			VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
			VkDevice logicalDevice = VK_NULL_HANDLE;
			VkSurfaceKHR presentSurface = VK_NULL_HANDLE;
			VmaAllocator memoryAllocator = VK_NULL_HANDLE;

			/// <summary>Creates the underlying Vulkan Instance w/ Required Extensions.</summary>
			void CreateVkInstance(const std::string& title) {
				VkApplicationInfo appInfo = {};
				appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
				appInfo.pApplicationName = title.c_str();
				appInfo.applicationVersion = TVK_RENDERER_VERSION;
				appInfo.pEngineName = TVK_RENDERER_NAME;
				appInfo.engineVersion = TVK_RENDERER_VERSION;
				appInfo.apiVersion = TVK_RENDERER_VERSION;

				VkInstanceCreateInfo createInfo = {};
				createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
				createInfo.pApplicationInfo = &appInfo;

				#if TVK_VALIDATION_LAYERS
					if (!QueryValidationLayerSupport())
						throw std::runtime_error("TinyVulkan: Failed to initialize validation layers!");

					VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
					debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
					debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
					debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
					debugCreateInfo.pfnUserCallback = DebugCallback;
					debugCreateInfo.pUserData = VK_NULL_HANDLE;

					createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
					createInfo.ppEnabledLayerNames = validationLayers.data();
					createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
				#endif

				std::vector<const char*> extensions(presentExtensionNames);
				for (const auto& extension : instanceExtensions) extensions.push_back(extension);

				createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
				createInfo.ppEnabledExtensionNames = extensions.data();

				VkResult result = vkCreateInstance(&createInfo, VK_NULL_HANDLE, &instance);
				if (result != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to create Vulkan instance! " + result);

				#if TVK_VALIDATION_LAYERS
					result = CreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, VK_NULL_HANDLE, &debugMessenger);
					if (result != VK_SUCCESS)
						throw std::runtime_error("TinyVulkan: Failed to set up debug messenger! " + result);

					for (const auto& extension : extensions) std::cout << '\t' << extension << '\n';
					std::cout << "TinyVulkan: " << extensions.size() << " extensions supported\n";
				#endif
			}
			
			/// <summary>Returns the first usable GPU.</summary>
			void QueryPhysicalDevice() {
				std::vector<VkPhysicalDevice> suitableDevices = QuerySuitableDevices();
				std::sort(suitableDevices.begin(), suitableDevices.end(), [](VkPhysicalDevice A, VkPhysicalDevice B) {
					VkPhysicalDeviceProperties2 propertiesA {};
					VkPhysicalDeviceProperties2 propertiesB {};
					propertiesA.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
					propertiesB.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
					vkGetPhysicalDeviceProperties2(A, &propertiesA);
					vkGetPhysicalDeviceProperties2(B, &propertiesB);
					return static_cast<size_t>(propertiesA.properties.deviceType) < static_cast<size_t>(propertiesB.properties.deviceType);
				});
				physicalDevice = (suitableDevices.size() > 0)? suitableDevices.front() : VK_NULL_HANDLE;

				if (physicalDevice == VK_NULL_HANDLE)
					throw std::runtime_error("TinyVulkan: Failed to find a suitable GPU!");
				
				#if TVK_VALIDATION_LAYERS
					VkPhysicalDevicePushDescriptorPropertiesKHR pushDescriptorProps = {};
					pushDescriptorProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR;

					VkPhysicalDeviceProperties2 deviceProperties = {};
					deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
					deviceProperties.pNext = &pushDescriptorProps;

					vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties);

					std::cout << "GPU Device Name:        " << deviceProperties.properties.deviceName << std::endl;
					std::cout << "Push Constant Memory:   " << deviceProperties.properties.limits.maxPushConstantsSize << " Bytes" << std::endl;
					std::cout << "Push Descriptor Memory: " << pushDescriptorProps.maxPushDescriptors << " Count" << std::endl;
				#endif
			}
			
			/// <summary>Creates the logical devices for the graphics/present queue families.</summary>
			void CreateLogicalDevice() {
				TinyVkQueueFamily indices = FindQueueFamilies();

				std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
				std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

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
					throw std::runtime_error("TinyVulkan: Failed to create logical device!");
			}
			
			/// <summary>Creates the VMAllocator for AMD's GPU memory handling API.</summary>
			void CreateVMAllocator() {
				VmaAllocatorCreateInfo allocatorCreateInfo = {};
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
				if (waitIdle) vkDeviceWaitIdle(logicalDevice);

				#if TVK_VALIDATION_LAYERS
					DestroyDebugUtilsMessengerEXT(instance, debugMessenger, VK_NULL_HANDLE);
				#endif
				
				vmaDestroyAllocator(memoryAllocator);
				vkDestroyDevice(logicalDevice, VK_NULL_HANDLE);
				vkDestroySurfaceKHR(instance, presentSurface, VK_NULL_HANDLE);
				vkDestroyInstance(instance, VK_NULL_HANDLE);
			}

			TinyVkVulkanDevice(const std::string title, const std::vector<VkPhysicalDeviceType> deviceTypes = { VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU }, TinyVkWindow* window = VK_NULL_HANDLE, const std::vector<const char*> presentExtensionNames = {}, VkPhysicalDeviceFeatures deviceFeatures = { .multiDrawIndirect = VK_TRUE }) : deviceTypes(deviceTypes), presentExtensionNames(presentExtensionNames) {
				onDispose.hook(TinyVkCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
				
				VkPhysicalDeviceFeaturesUnionArray featuresA = { .vkfeatures = this->deviceFeatures }, featuresB = { .vkfeatures = deviceFeatures };
				for(size_t i = 0; i < sizeof(VkPhysicalDeviceFeatures)/sizeof(VkBool32); i++) featuresA.features[i] |= featuresB.features[i];
				this->deviceFeatures = featuresA.vkfeatures;

				CreateVkInstance(title);

				if (window != VK_NULL_HANDLE) {
					presentSurface = window->CreateWindowSurface(instance);

					#ifdef TINYVK_AUTO_PRESENT_EXTENSIONS
					for (auto str : TinyVkWindow::QueryRequiredExtensions(TVK_VALIDATION_LAYERS))
						this->presentExtensionNames.push_back(str);
					#endif
				}

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

			#pragma endregion
			#pragma region VULKAN_VALIDATION_LAYERS
			
			/// <summary>Queries device drivers for installed Validation Layers.</summary>
			bool QueryValidationLayerSupport() {
				uint32_t layersFound = 0, layersCount = 0;
				vkEnumerateInstanceLayerProperties(&layersCount, VK_NULL_HANDLE);
				std::vector<VkLayerProperties> availableLayers(layersCount);
				vkEnumerateInstanceLayerProperties(&layersCount, availableLayers.data());

				for (const auto& layerProperties : availableLayers) {
					#if TVK_VALIDATION_LAYERS
					std::cout << layerProperties.layerName << std::endl;
					#endif

					for (const std::string layerName : validationLayers)
						if (!layerName.compare(layerProperties.layerName)) layersFound++;
				}

				return (layersFound == validationLayers.size());
				
			}

			#pragma endregion
			#pragma region VULKAN_GPU_DEVICE

			/// <summary>Wait for GPU device to finish transfer/render commands.</summary>
			inline VkResult DeviceWaitIdle() { return vkDeviceWaitIdle(logicalDevice); }

			/// <summary>Returns info about the VkPhysicalDevice graphics/present queue families. If no surface provided, auto checks for Win32 surface support.</summary>
			TinyVkQueueFamily FindQueueFamilies(VkPhysicalDevice newDevice = VK_NULL_HANDLE) {
				VkPhysicalDevice device = (newDevice == VK_NULL_HANDLE)? physicalDevice : newDevice;

				uint32_t queueFamilyCount = 0;
				vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, VK_NULL_HANDLE);
				std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
				vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

				TinyVkQueueFamily indices;
				VkBool32 presentSupport = presentSurface != VK_NULL_HANDLE;
				for (int i = 0; i < queueFamilies.size(); i++) {
					vkGetPhysicalDeviceSurfaceSupportKHR(device, i, presentSurface, &presentSupport);
					indices.graphicsFamily = (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)? i : indices.graphicsFamily;
					indices.presentFamily = (presentSupport)? i: indices.presentFamily;
					if (indices.HasGraphicsFamily() || (!presentSupport && (presentSupport && indices.HasPresentFamily()))) break;
				}

				return indices;
			}

			/// <summary>Checks the VkPhysicalDevice for swap-chain availability.</summary>
			TinyVkSwapChainSupporter QuerySwapChainSupport(VkPhysicalDevice device) {
				TinyVkSwapChainSupporter details;

				uint32_t formatCount;
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, presentSurface, &formatCount, VK_NULL_HANDLE);
				details.formats.resize(formatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, presentSurface, &formatCount, details.formats.data());
				vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, presentSurface, &details.capabilities);

				uint32_t presentModeCount;
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, presentSurface, &presentModeCount, VK_NULL_HANDLE);
				details.presentModes.resize(presentModeCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, presentSurface, &presentModeCount, details.presentModes.data());
				return details;
			}

			/// <summary>Returns BOOL(true/false) if the VkPhysicalDevice (GPU/iGPU) supports extensions.</summary>
			bool QueryDeviceExtensionSupport(VkPhysicalDevice device) {
				uint32_t extensionCount;
				vkEnumerateDeviceExtensionProperties(device, VK_NULL_HANDLE, &extensionCount, VK_NULL_HANDLE);
				std::vector<VkExtensionProperties> availableExtensions(extensionCount);
				vkEnumerateDeviceExtensionProperties(device, VK_NULL_HANDLE, &extensionCount, availableExtensions.data());
				
				std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
				for (const auto& extension : availableExtensions)
					requiredExtensions.erase(extension.extensionName);

				#if TVK_VALIDATION_LAYERS
					for (const auto& extension : requiredExtensions)
						std::cout << "UNAVAILABLE EXTENSIONS: " << extension << std::endl;
				#endif

				return requiredExtensions.empty();
			}

			/// <summary>Returns BOOL(true/false) if a physical device is compatible with the required vulkan renderer (with or wihtout present support).</summary>
			bool QueryDeviceCompatibility(VkPhysicalDevice device) {
				VkPhysicalDeviceProperties2 deviceProperties {};
				deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
				vkGetPhysicalDeviceProperties2(device, &deviceProperties);
				
				VkPhysicalDeviceFeatures deviceFeatures {};
				vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
				
				TinyVkQueueFamily indices = FindQueueFamilies(device);
				bool supportsExtensions = QueryDeviceExtensionSupport(device);

				TinyVkSwapChainSupporter swapChainSupport = QuerySwapChainSupport(device);
				bool swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
				bool requiresPresent = (presentSurface == VK_NULL_HANDLE) || (presentSurface != VK_NULL_HANDLE && indices.HasPresentFamily() && swapChainAdequate);
				
				bool hasType = false;
				for (auto type : deviceTypes)
					hasType = (!hasType)? (deviceProperties.properties.deviceType == type) : hasType;
				
				bool hasFeatures = true;
				VkPhysicalDeviceFeaturesUnionArray featuresA = { .vkfeatures = this->deviceFeatures }, featuresB = { .vkfeatures = deviceFeatures };
				for(size_t i = 0; i < sizeof(VkPhysicalDeviceFeatures)/sizeof(VkBool32); i++)
					if (featuresA.features[i] && !featuresB.features[i]) hasFeatures = false;

				return indices.HasGraphicsFamily() && hasType && supportsExtensions && requiresPresent && hasFeatures;
			}

			/// <summary>Returns a Vector of suitable VkPhysicalDevices (GPU/iGPU).</summary>
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