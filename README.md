# TinyVK (Simple Vulkan Renderer)

***Required Minimum Vulkan SDK: 1.3.268.0***

This is a fork of [TinyVulkan-Dynamic (e3339c9)](https://github.com/Yaazarai/TinyVulkan-Dynamic/commit/e3339c93c33d895a323ce0acade4df3e224c8769). The TinyVulkan API is a simple Vulkan Renderer that provides all of the basics required for simple (non-compute, non-RTX) graphics rendering projects--graphics pipeline does not support RT/COMPUTE shaders. You can download the source headers from the [./TinyVulkan/](https://github.com/Yaazarai/TinyVK/tree/main/TinyVulkan) folder and drop them into your project. Follow the required compiler/linker options in the `TinyVulkan.hpp` header file and you're good to start developing.

Provided however is a batch script development environment and sample `source.cpp` project which renders a single moving color-interpolated quad (requiring you to install the ***clang-cl/LLVM*** compiler/linker) which you can run using `buildtools.bat` to open a console window and then run any of the `_DEBUG`/`_RELEASE`/`_SHADERS` build commands. Please make sure to set your own library paths for LLVM/VULKAN/GLFW/VMA and any other custom library folders within the `buildtools.bat` batch file before compiling. Running the build commands outside of this batch development environment will fail as they do not have the require environment library path variables.

### Why TinyVK (TinyVulkan)?

Tiny-Vulkan is a great simple renderer (with default 2D camera projection matrix) designed for rapid C++ graphics development, easily compatible with ImGUI for UI applications if needed or standalone for game development. This was not designed for large 3D Voxel rendering projects or the like complexity.

Look how smooth TinyVK is too...
<p align="center">
  <img src="https://i.imgur.com/wAkzzrK.gif" />
</p>

### API Structure

Similar to actual Vulkan in order to get a working renderer that presents to a window you create a `TinyVkWindow`, `TinyVkVulkanDevice` -> `TinyVkCommandPool` (for general buffer/image transfer commands) -> `TinyVkGraphicsPipeline` then you can create the `TinyVkSwapChainRenderer` for presenitng ot the screen. In order to create render functions/callbacks you must register render events to the `onRenderEvents` invokable event in either the `TinyVkImageRenderer` or `TinyVkSwapChainRenderer` which will provide a command pool that you can "rent," buffers from to write commands to. Any buffer rented from the provided command pool should NOT be returned to the command pool--as the renderer uses "rented," command buffers during render execution. You can rent out as many command buffers as you want, so long as you've provided a large neough pool when created your renderer. The general layout for creating render events is as follows: hook callback to onRenderEvents, callback accepts reference to command pool, in callback rent command buffers, begin recording command buffer, register render calls, end recording command buffer, then in your main while loop call `RenderExecute()` (see renderer documentation below). Here is an example, also seen below:
```
swapRenderer.onRenderEvents.hook(TinyVkCallback<TinyVkCommandPool&>([&angle, &window, &swapRenderer, &pipeline, &queue, &clearColor, &depthStencil, &offsets](TinyVkCommandPool& commandPool) {
    auto frame = queue.GetFrameResource();

    auto commandBuffer = commandPool.LeaseBuffer();
    swapRenderer.BeginRecordCmdBuffer(commandBuffer.first, clearColor, depthStencil);
    
    int offsetx, offsety;
    offsetx = glm::sin(glm::radians(static_cast<glm::float32>(angle))) * 64;
    offsety = glm::cos(glm::radians(static_cast<glm::float32>(angle))) * 64;
    glm::mat4 camera = TinyVkMath::Project2D(window.GetWidth(), window.GetHeight(), offsetx, offsety, 1.0, 0.0);
    frame.projection.StageBufferData(&camera, sizeof(glm::mat4), 0, 0);
    VkDescriptorBufferInfo cameraDescriptorInfo = frame.projection.GetBufferDescriptor();
    VkWriteDescriptorSet cameraDescriptor = pipeline.SelectWriteBufferDescriptor(0, 1, { &cameraDescriptorInfo });
    swapRenderer.PushDescriptorSet(commandBuffer.first, { cameraDescriptor });
    
    swapRenderer.CmdBindGeometry(commandBuffer.first, &frame.vbuffer.buffer, frame.ibuffer.buffer, offsets, offsets[0]);
    swapRenderer.CmdDrawGeometry(commandBuffer.first, true, 1, 0, 6, 0, 0);
    swapRenderer.EndRecordCmdBuffer(commandBuffer.first, clearColor, depthStencil);

    angle += 1.25;
}));
```
The `TinyVkCallback<TinyVkCommandPool&>` accepts a lambda, static or object function so long as the function matches the templated arguments of the callback, in this case `<TinyVkCommandPool&>`. You can also provide a capture list of instanced objects to use within your lambda render call. This example creates a render callback for the `TinyVkCSwapChainRenderer` that writes a UBO push descriptor of type `glm::mat4` to the shader (for camera transforms) and then binds the quad geometry buffers and finally draws the quad within the bound buffer.

## TinyVK API

Please read the `TinyVulkan.hpp` file for compiler/linker information. Here are the following default options for VULKAN / VMA / GLFW / GLM:
```
* #define _DEBUG to enable console & validation a layers.
* #define _RELEASE for an optimized window GUI only build.
* You can #define TINYVULKAN_NAMESPACE before including TinyVulkan to override the default [tinyvk] namespace.
* The TINYVK_VALIDATION_LAYERS macros tell you true/false whether validation layers are enabled.
* GLM forces left-handle axis--top-left is 0,0 (GLM_FORCE_LEFT_HANDED).
* GLM forces depth range zero to one (GLM_FORCE_DEPTH_ZERO_TO_ONE).
* GLM forces memory aligned types (GLM_FORCE_DEFAULT_ALIGNED_GENTYPES).
* #define TINYVK_ALLOWS_POLLING_GAMEPADS to make the Window check for gamepad inputs.
* Library can be compiled with either native MSVC (Visual Studio) or clang-cl (LLVM MSVC).
* The Vulkan SDK includes VMA by default and does not need to be imcluded separately.
* Compile with _RELEASE and /CONSOLE (or /subsystem:console) to create a release build with console access & validation layers.

* Default Validation Layers: VK_LAYER_KHRONOS_validation
* Default Device Extensions:
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,             // Swapchain support for buffering frame images with the device driver to reduce tearing.
	VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,   // Dynamic Rendering Dependency.
	VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, // Dynamic Rendering Dependency.
	VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,     // Allows for rendering without framebuffers and render passes.
	VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME        // Allows for writing descriptors directly into a command buffer rather than allocating from sets / pools.
```

* **TinyVK_TimedGuard.hpp**: provides a timeout based `std::lock_guard` called `timed_guard<bool,size_t>` implementation and functions similarily, accepting an `std::timed_mutex` and template arguments `<bool wait, size_t timeout` specifiy if the timed guard should wait on a mutex and for howlong that timeout wait should be in milliseconds. You can call `bool Acquired()` to check the acquired status of the mutex and `void Unlock()` to release the mutex.

* **TinyVk_Invokable.hpp**: provides an invokable-callback event pattern. You can create an `TinyVkCallback<T>` to represent a function callback and hook it into an event `TinyVkInvokable<TinyVkCallback<T>>` to be called later. See: [Event-Callback](https://github.com/Yaazarai/Event-Callback) for more information.

* **TInyVk_Utilities**: provides cross-api utilities and Vulkan Debug/Render function loaders. Also provided is the `TinyVkRendererInterface` which is simply a backend interface of supporting `vkCmd*` rendering functions provided to the TinyVk renderers. Then `TinyVkBufferingMode` enum for specifying the screen buffering mode when creating a SwapChain renderer (window renderer). Then `TinyVkSwapChainSUpporter` struct used internally for storing certain SwapChain formatting requirements. Then `TinyVkSurfaceSupporter` which can be passed to `TinyVkSwapChainRenderer` to specify the output format of your renderer--default settings provided:
```
VkFormat dataFormat = VK_FORMAT_B8G8R8A8_UNORM;
VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
VkPresentModeKHR idealPresentMode = VK_PRESENT_MODE_FIFO_KHR;
```

* **TinyVk_Disposable.hpp**: provides a disposable class interface for easily managing dispose events for freeing up dynamic resources. You can assign this to any class, then hook a `TinyVkCallback<bool>` function callback onto the `onDispose` event which call call the cleanup function appropriately when the class object goes out of scope.

* **TinyVk_InputEnums.hpp**: Is a drop-in replacement for the GLFW input macro values for easy referencing with the TInyVk input API provided by `TinyVkWindow`.

* **TinyVk_Window.hpp**: provides the `TinyVkWindow` GLFW window implementation for single-window rendering (multi-window support not provided, but could be easily implemented). This also includes `TinyVkInvokable` GLFW input events for keyboard, mouse and gamepads to detect user input. This implementation also provides a default `WhileMain(bool)` function which accepts BOOL of TRUE to call `glfwWaitEvents()` or FALSE `glfwPollEvents()`. Optionally you can `#define TINYVK_ALLOWS_POLLING_GAMEPADS` before including TinyVulkan to tell the GLFW to poll for gamepad inputs. The WHileMain function provides its own event which you can register `TInyVkCallback` functions to run your own render functions or other logic during execution. You can call the WhileMain function or optionally call it with a render thread on the side:
```
//OPTION-A: Single-Threaded Window Renderer: (render events hang on logic/polling execution)
window.onWhileMain.hook(
        TinyVkCallback<std::atomic_bool&>([&window, &swapRenderer](std::atomic_bool& wait) { while (!window.ShouldClose()) { swapRenderer.RenderExecute(); } })
);
window.WhileMain(true);

//OPTION-B: Dual-Threaded Window Renderer: (render/logic/polling happen in parallel)
std::thread mythread([&window, &swapRenderer]() { while (!window.ShouldClose()) { swapRenderer.RenderExecute(); } });
window.WhileMain(true);
mythread.join();
```

* **TinyVk_VulkanDevice.hpp**: provides the `TinyVkVulkanDevice` which initializes the Vulkan API/Drivers and creates a Vulkan logical device for rendering. If you wish to present to the screen you must pass a `TinyVkWindow` object pointer for creating a Vulkan render presentation surface for the Window. If you pass `VK_NULL_HANDLE` or `nullptr` and no presentation extensions, then you can use the VulkanDevice in headless rendering mode (no GUI, console only). The `TinyVkWindow` provides a default function for getting present extensions: `QueryRequiredExtensions(enable validation layers?)` which can be called using the VALIDATION LAYERS macro:
```
window.QueryRequiredExtensions(TINYVK_VALIDATION_LAYERS)
```

* **TinyVk_CommandPool.hpp**: provides the `TinyVkCommandPool` which creates a `VkCommandPool` and tracks which `vkCommandBuffers` are in-use using a rent/return ID tracking. You may need to create a command pool for your own render commands as needed.

* **TinyVk_GraphicsPipeline.hpp**: provides the `TinyVkGraphicsPipeline` which defines how your graphics will be renderer, such as vertex/polgyon topology, image format, color components, depth buffering and loading shaders. When creating a graphics pipeline you must pass an array of any PushDescriptor or PushConstant layouts required by your shaders or an empty `{}` array if unused. The pipeline provides the `SelectPushConstantRange()` and `SelectPushDescriptorLayoutBinding()` for creating your layout bindings. Then the pipeline provides the SelectWrite*() functions for getting the write descriptors for writing CPU data to your GPU push descriptors. For Push Descriptors model will always follow `Get*Descriptor(...)` then `SelectWrite*Descriptor(...)` and finally `PushDescriptorSet(...)`. Example, which passes a mat4 camera matrix as a UBO buffer or Push Constant to the vertex shader:
```
glm::mat4 camera = TinyVkMath::Project2D(window.GetWidth(), window.GetHeight(), offsetx, offsety, 1.0, 0.0);
frame.projection.StageBufferData(&camera, sizeof(glm::mat4), 0, 0);

// Push Dexcriptors:
VkDescriptorBufferInfo cameraDescriptorInfo = frame.projection.GetBufferDescriptor();
VkWriteDescriptorSet cameraDescriptor = pipeline.SelectWriteBufferDescriptor(0, 1, { &cameraDescriptorInfo });
renderer.PushDescriptorSet(commandBuffer.first, { cameraDescriptor });

// Push Constants:
renderer.PushConstants(commandBuffer, VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), &camera);
```

* **TinyVk_Buffer.hpp**: provides the `TinyVkBuffer` which will utilize VMA to allocate the GPU side memory buffer for sending UBO data to the GPU. You can `StageBufferData()` to send data to the GPU, `TransferBufferCmd()` to copy data from one buffer to another or `GetBufferDescriptor()` when pushing the buffer to the GPU as a Push Descriptor.

* **TinyVk_Image.hpp**: provides the `TinyVkImage` which will utilize VMA to allocate the GPU side memory image for rendering data to or for passing textures to shaders. You can `StageImageData()` to copy a CPU image to GPU texture memory, `TransferFromBufferCmd()` to copy data from one buffer to the GPU image or `GetImageDescriptor()` when pushing the image to the GPU as a Push Descriptor. Finally call `ReCreateImage()` to re-use this `TinyVkImage` object and recreate its underlying image using different formatting. The `TinyVkImage` can also be used as a render target for the `TinyVkImageRenderer` for the render-to-texture model.

* **TinyVk_ImageRender.hpp**: provides the `TinyVkImageRenderer` for rendering directly to a GPU texture image instead of presenting directly to the screen. It takes an initial "render target" (`TinyVkImage`) that you can render to or swap render targets by calling `SetRenderTarget()`. You create your image renderer, hook a render event `TinyVkCallback` to `onRenderEvents` and voila, you can render to your target image. Call `RenderExecute(VkCommandBuffer)` to begin rendering. If you have hooked a render event you can leave the command buffer parameter as `VK_NULL_HANDLE` or empty--defaults to null--if you have not hooked an event, you can pass a command buffer with pre-recorded commands. Example:
```
TinyVkVulkanDevice vkdevice("Sample Application", { VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU });
TinyVkGraphicsPipeline pipeline(vkdevice, VK_FORMAT_B8G8R8A8_UNORM, vertexDescription, defaultShaders, pushDescriptorLayouts, {}, false);
TinyVkImage image(vkdevice, pipeline, cmdpool, width, height, false /*isdepthimage*/, VK_FORMAT_B8G8R8A8_SRGB, TINYVK_UNDEFINED, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_ASPECT_COLOR_BIT);
TinyVkImageRenderer imageRenderer(vkdevice, &image, pipeline, cmdBufferCount = 32 /*default*/);
imageRenderer.onRenderEvents.hook(
    TinyVkCallback<TinyVkCommandPool&>([](TinyVkCommandPool& cmdPool){
        auto commandBuffer = commandPool.LeaseBuffer();
        imageRenderer.BeginRecordCmdBuffer(commandBuffer.first, clearColor, depthStencil);
        // Render Commands Here...
        // Render Commands Here...
        // Render Commands Here...
        imageRenderer.EndRecordCmdBuffer(commandBuffer.first, clearColor, depthStencil);
    })
);
imageRenderer.RenderExecute();
```

* **TinyVk_SwapChainRenderer.hpp**: provides the `TinyVkSwapChainRenderer` which functions largely similarly to the `TinyVkImageRenderer` except will render and present to a swapchain and thus to a window instead of a texture. Example:
```
TinyVkWindow window("Sample Application", 1440, 810, true, false);
TinyVkVulkanDevice vkdevice("Sample Application", { VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU }, &window, window.QueryRequiredExtensions(TINYVK_VALIDATION_LAYERS));
TinyVkCommandPool commandPool(vkdevice, DEFAULT_COMMAND_POOLSIZE);
TinyVkGraphicsPipeline pipeline(vkdevice, VK_FORMAT_B8G8R8A8_UNORM, vertexDescription, defaultShaders, pushDescriptorLayouts, {}, false);
TinyVkSwapChainRenderer swapRenderer(vkdevice, window, pipeline, bufferingMode);

swapRenderer.onRenderEvents.hook(
    TinyVkCallback<TinyVkCommandPool&>([](TinyVkCommandPool& cmdPool){
        auto commandBuffer = commandPool.LeaseBuffer();
        swapRenderer.BeginRecordCmdBuffer(commandBuffer.first, clearColor, depthStencil);
        // Render Commands Here...
        // Render Commands Here...
        // Render Commands Here...
        swapRenderer.EndRecordCmdBuffer(commandBuffer.first, clearColor, depthStencil);
    })
);
swapRenderer.RenderExecute();
```

The `TinyVkImageRenderer` and `TinyVkSwapRenderer` implement the same supporting backend interface `TinyVkRendererInterface` providing the following render functions:
```
/// <summary>Begins recording render commands to the provided command buffer.</summary>
BeginRecordCmdBuffer(const VkClearValue clearColor, const VkClearValue depthStencil, VkCommandBuffer commandBuffer)

/// <summary>Ends recording render commands to the provided command buffer.</summary>
EndRecordCmdBuffer(const VkClearValue clearColor, const VkClearValue depthStencil, VkCommandBuffer commandBuffer)

/// <summary>Alias call for easy-calls to: vkCmdBindVertexBuffers + vkCmdBindIndexBuffer.</summary>
CmdBindGeometry(VkCommandBuffer cmdBuffer, const VkBuffer* vertexBuffers, const VkBuffer indexBuffer, const VkDeviceSize* offsets, const VkDeviceSize indexOffset = 0, uint32_t binding = 0, uint32_t bindingCount = 1)

/// <summary>Records Push Descriptors to the command buffer.</summary>
VkResult PushDescriptorSet(VkCommandBuffer cmdBuffer, std::vector<VkWriteDescriptorSet> writeDescriptorSets)

/// <summary>Records Push Constants to the command buffer.</summary>
PushConstants(VkCommandBuffer cmdBuffer, VkShaderStageFlagBits shaderFlags, uint32_t byteSize, const void* pValues)

/// <summary>Executes the registered onRenderEvents and renders them to the target image/texture.</summary>
RenderExecute(VkCommandBuffer preRecordedCmdBuffer = VK_NULL_HANDLE /*ONly for TinyVkImageRenderer*/)

/// <summary>Alias call for: vkCmdBindVertexBuffers.</summary>
CmdBindGeometry(VkCommandBuffer cmdBuffer, const VkBuffer* vertexBuffers, const VkDeviceSize* offsets, uint32_t binding = 0, uint32_t bindingCount = 1)

/// <summary>Alias call for: vkCmdBindIndexBuffers.</summary>
CmdBindGeometry(VkCommandBuffer cmdBuffer, const VkBuffer indexBuffer, const VkDeviceSize indexOffset = 0, uint32_t binding = 0, uint32_t bindingCount = 1)

/// <summary>Alias call for: vkCmdBindVertexBuffers2.</summary>
CmdBindGeometry(VkCommandBuffer cmdBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* vertexBuffers, const VkDeviceSize* vbufferOffsets, const VkDeviceSize* vbufferSizes, const VkDeviceSize* vbufferStrides = VK_NULL_HANDLE)

/// <summary>Alias call for vkCmdDraw (isIndexed = false) and vkCmdDrawIndexed (isIndexed = true).</summary>
CmdDrawGeometry(VkCommandBuffer cmdBuffer, bool isIndexed = false, uint32_t instanceCount = 1, uint32_t firstInstance = 0, uint32_t vertexCount = 0, uint32_t vertexOffset = 0, uint32_t firstIndex = 0)

/// <summary>Alias call for: vkCmdDrawIndexedIndirect.</summary>
CmdDrawGeometryIndirect(VkCommandBuffer cmdBuffer, const VkBuffer drawParamBuffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)

/// <summary>Alias call for: vkCmdDrawIndexedIndirectCount.</summary>
CmdDrawGeometryIndirect(VkCommandBuffer cmdBuffer, const VkBuffer drawParamBuffer, VkDeviceSize offset, const VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t drawCount, uint32_t maxDrawCount, uint32_t stride)
```

* **TinyVk_ResourceQueue.hpp**: provides a templated queue that you can fill with resources and pull on a given frame index when rendering to multiple images or to the swapchain. Give an IndexCallback (to get the frame index) and a DestructorCallback (to free resources automatically) along with an array of your resources to easily manage rendering to several render targets. Note that this is necessary when rendering to the Swap Chain (presenting to window) because one swap chain image may be rendering when we need to update data for the next frame. The following example creates a frame resource struct `SwapFrame` to represent our per-frame vertex/index/projection buffer resources and passes our resources to our resource queue, then creates the necessary callbacks for indexing/destruction:
```
std::vector<TinyVkVertex> triangles = {
        TinyVkVertex({0.0f,0.0f}, {240.0f,135.0f,               1.0f}, {1.0f,0.0f,0.0f,1.0f}),
        TinyVkVertex({0.0f,0.0f}, {240.0f+960.0f,135.0f,        1.0f}, {0.0f,1.0f,0.0f,1.0f}),
        TinyVkVertex({0.0f,0.0f}, {240.0f+960.0f,135.0f+540.0f, 1.0f}, {1.0f,0.0f,1.0f,1.0f}),
        TinyVkVertex({0.0f,0.0f}, {240.0f,135.0f + 540.0f,      1.0f}, {0.0f,0.0f,1.0f,1.0f})
    }; std::vector<uint32_t> indices = {0,1,2,2,3,0};

    TinyVkBuffer vbuffer(vkdevice, pipeline, commandPool, triangles.size() * sizeof(TinyVkVertex), TinyVkBufferType::VKVMA_BUFFER_TYPE_VERTEX);
    vbuffer.StageBufferData(triangles.data(), triangles.size() * sizeof(TinyVkVertex), 0, 0);
    TinyVkBuffer ibuffer(vkdevice, pipeline, commandPool, indices.size() * sizeof(indices[0]), TinyVkBufferType::VKVMA_BUFFER_TYPE_INDEX);
    ibuffer.StageBufferData(indices.data(), indices.size() * sizeof(indices[0]), 0, 0);
    TinyVkBuffer projection1(vkdevice, pipeline, commandPool, sizeof(glm::mat4), TinyVkBufferType::VKVMA_BUFFER_TYPE_UNIFORM);
    TinyVkBuffer projection2(vkdevice, pipeline, commandPool, sizeof(glm::mat4), TinyVkBufferType::VKVMA_BUFFER_TYPE_UNIFORM);

struct SwapFrame {
    TinyVkBuffer &projection, &ibuffer, &vbuffer;
    SwapFrame(TinyVkBuffer& projection, TinyVkBuffer& ibuffer, TinyVkBuffer& vbuffer) : projection(projection), ibuffer(ibuffer), vbuffer(vbuffer) {}
};

TinyVkResourceQueue<SwapFrame,static_cast<size_t>(bufferingMode)> queue(
    {
        SwapFrame(projection1,ibuffer,vbuffer),
        SwapFrame(projection2,ibuffer,vbuffer)
    },
    TinyVkCallback<size_t&>([&swapRenderer](size_t& frameIndex){ frameIndex = swapRenderer.GetSyncronizedFrameIndex(); }),
    TinyVkCallback<SwapFrame&>([](SwapFrame& resource){})
);

swapRenderer.onRenderEvents.hook(.... {
    // Now get the resources for the current frame.
    auto frame = queue.GetFrameResource();
}));
```

* **TinyVk_VertexMath.hpp**: provides a default vertex implementation (optional) for use with your graphics pipeline called `TinyVkVertex` see implementation if you want to write your own custom implementation, providing the following default data format: `vec2(RG32) texcoord, vec3(RGB32) position, vec4(RGBA32) color`. `TinyVkMath` provides static function calls for camera projection, converting XY/UV coordinates and angle function helpers. Lastly `TinyVkQuad` provides helper function when used with `TinyVkVertex` to easily create/scale/rotate/offset quads for rendering (vertex order is top-left, top-right, bottom-right, bottom-left).
