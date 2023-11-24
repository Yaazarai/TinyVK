# TinyVulkan
TinyVulkan API is written for learning purposes as well as purposefully built for rapid application and 2D game development. Vulkan graphical render engine built using Dynamic Rendering / Push Constants / Push Descriptors for an easy to understand full implementation.

VULKAN RENDER DEVICE EXTENSIONS:
```
VK_KHR_SWAPCHAIN_EXTENSION_NAME
VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME
VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME
VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME
```

The API provides the following class header implementations denoted by (`*`):
```
Tiny_VkTimedGuard.hpp
    // timed_guard is the only exception to the naming paradigm as a timeout replacement for std::lock_guard.
    * timed_guard
* TinyVkInvokable
Tiny_VkUtilities.hpp
    * TinyVkBufferingMode
    * TinyVkSwapChainSupporter
    * TinyVkSurfaceSupporter
    * TinyVkRendererInterface
* TinyVkDisposable
* TinyVkInputEnums
* TinyVkWindow
* TinyVkVulkanDevice
* TinyVkCommandPool
* TinyVkGraphicsPipeline
* TinyVkBuffer
* TinyVkImage
* TinyVkImageRenderer
* TinyVkSwapChainRenderer
* TinyVkResourceQueue
TinyVk_VertexMath.hpp
    * TinyVkVertex
    * TinyVkMath
    * TinyVkQuad
```


## Implementation Design (Ordered by Dependency)

**TinyVk_TimedGuard.hpp**
* timed_guard `->` A simple time-out based implementation for `std::lock_guard` allows you to timeout mutex lock acquiring.
  * `bool Acquired()` hecks if the mutex has been acquired or not.
  * `void Unlock()` unlocks the **acquired** mutex.

**TinyVk_Invokable.hpp**
* `TinyVkCallback` and `TinyVkInvokable` see the following for documentation: https://github.com/Yaazarai/Event-Callback

**TinyVk_Utilities.hpp**
* `CreateDebugUtilsMessengerEXT`, `DestroyDebugUtilsMessengerEXT` and `DebugCallback` for the VkInstance (Vulkan Driver Instance) validation layers / error checking.
* `vkCmdRenderingGetCallbacks`, `vkCmdBeginRenderingEKHR`, `vkCmdEndRenderingEKHR` and `vkCmdPushDescriptorSetEKHR` are driver loading/loaded extension functions for `vkCmdBeginRenderingKHR/vkCmdEndRenderingKHR/vkCmdPushDescriptorSetKHR`.
* `TinyVkBufferingMode` represents the number of images available for screen buffering/refreshing (2, 3 or 4). Any less than 2 is not provided and will result in an error.
* `*TinyVkSwapChainSupporter` supporter class for dealing with the SwapChain surface formatting and capabilities during Vulkan device and swapchain creation (primarily internal usage).
* `TinyVkSurfaceSupporter` supporter class when creating a `TinyVkSwapChainRenderer` to specify the image format (defaults to BGRA-8 SRGB UNORM), colorspace (BGRA-8 SRGB NONLINEAR) and present mode (how buffered images are displayed) for rendering to the screen.
* `*TinyVkRendererInterface` is an interface class that provides alternative default rendering functions shared by both `TinyVkImageRenderer` and `TinyVkSwapChainRenderer`.

**TinyVk_Disposable.hpp**
* `TinyVkDisposable` interface allows you to create objects that require disposing of dynamic memory resources reliably and with ease through event queueing. Assign this to your class and HOOK a dispose event to `onDispose` for cleanly disposing of dynamic memory resources.

**TinyVk_InputENums.hpp**
* `TinyVkModKeyBits`, `TinyVkKeyboardButtons`, `TinyVkMouseButtons`, `TinyVkGamepadButtons`, `TinyVkGamepadAxis`, `TinyVkGamepads` and `TinyVkInputEvents` are ENUM replacements for GLFW's macro based C-implementation for user device inputs.

**TinyVk_Window.hpp**
* `TinyVk_Window` provides a GLFW based window implementation.

IN PROGRESS...
