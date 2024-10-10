#pragma once
#ifndef TINYVULKAN_LIB
#define TINYVULKAN_LIB

    /*
       TINYVULKAN LIBRARY DEPENDENCIES: VULKAN and GLFW compiled library binaries.

        C/C++ | Code Configuration | Runtime Libraries:
            RELEASE: /MD
            DEBUG  : /mtD
        C/C++ | General | Additional Include Directories: (DEBUG & RELEASE)
            ..\VulkanSDK\1.3.239.0\Include;...\glfw-3.3.7.bin.WIN64\glfw-3.3.7.bin.WIN64\include;
        Linker | General | Additional Library Directories: (DEBUG & RELEASE)
            ...\glfw-3.3.7.bin.WIN64\lib-vc2022;$(VULKAN_SDK)\Lib; // C:\VulkanSDK\1.3.239.0\Lib
        Linker | Input | Additional Dependencies:
            RELEASE: vulkan-1.lib;glfw3.lib;
            DEBUG  : vulkan-1.lib;glfw3_mt.lib;
        Linker | System | SubSystem
            RELEASE: Windows (/SUBSYSTEM:WINDOWS)
            DEBUG  : Console (/SUBSYSTEM:CONSOLE)

        ** If Debug mode is enabled the console window will be visible, but not on Release.
        **** Make sure you've installed the Vulkan SDK binaries.
        **** Make sure you've installed the GLFW SDK binaries.
        
        DEVICE EXTENSIONS:
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,             // Swapchain support for buffering frame images with the device driver to reduce tearing.
			VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,   // Dynamic Rendering Dependency.
			VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, // Depth fragment testing (discard fragments if fail depth test, if enabled in the graphics pipeline).
			VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,     // Allows for rendering without framebuffers and render passes.
			VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME        // Allows for writing descriptors directly into a command buffer rather than allocating from sets / pools.

        Allows the window to poll gamepad inputs:
            #define TINYVK_ALLOWS_POLLING_GAMEPADS
        
        Auto inserts window instance extensions:
            #define TINYVK_AUTO_PRESENT_EXTENSIONS
    */

    #define GLFW_INCLUDE_VULKAN
    #if defined (_WIN32)
        #define GLFW_EXPOSE_NATIVE_WIN32
        #define VK_USE_PLATFORM_WIN32_KHR
    #endif
    #include <GLFW/glfw3.h>
    #include <GLFW/glfw3native.h>
    #include <vulkan/vulkan.h>
    
    #ifdef _DEBUG
        #define TINYVULKAN_WINDOWMAIN main(int argc, char* argv[])
        #define TVK_VALIDATION_LAYERS VK_TRUE
    #else
        #define TVK_VALIDATION_LAYERS VK_FALSE
        #ifdef _WIN32
			#ifndef __clang__
			// COMPILING WITH VISUAL STUDIO 2022
            #define TINYVULKAN_WINDOWMAIN __stdcall wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
			#else
			// COMPILING WITH CLANG-CL / LLVM
            #define TINYVULKAN_WINDOWMAIN __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
            #endif
        #else
            // For debugging w/ Console on Release change from /WINDOW to /CONSOLE: Linker -> System -> Subsystem.
            #define TINYVULKAN_WINDOWMAIN main(int argc, char* argv[])
        #endif
    #endif
    #ifndef TINYVULKAN_NAMESPACE
        #define TINYVULKAN_NAMESPACE tinyvk
        namespace TINYVULKAN_NAMESPACE {}
    #endif
    #define TINYVK_VALIDATION_LAYERS TVK_VALIDATION_LAYERS

    #define TVK_MAKE_VERSION(variant, major, minor, patch) ((((uint32_t)variant)<<29)|(((uint32_t)major)<<22)|(((uint32_t)minor)<<12)|((uint32_t)patch))
    #define TVK_RENDERER_VERSION TVK_MAKE_VERSION(0, 1, 1, 0)
    #define TVK_RENDERER_NAME "TINYVULKAN_LIBRARY"

    #define VMA_IMPLEMENTATION
    #define VMA_DEBUG_GLOBAL_MUTEX VK_TRUE
    #define VMA_USE_STL_CONTAINERS VK_TRUE
    #define VMA_RECORDING_ENABLED TVK_VALIDATION_LAYERS
    #include <vma/vk_mem_alloc.h>

    #define GLM_FORCE_RADIANS
    #define GLM_FORCE_LEFT_HANDED
    #define GLM_FORCE_DEPTH_ZERO_TO_ONE
    #define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
    #include <glm/glm.hpp>
    #include <glm/ext.hpp>

    #include <fstream>
    #include <iostream>
    #include <array>
    #include <set>
    #include <optional>
    #include <string>
    #include <vector>
    #include <algorithm>

    #pragma region BACKEND_SYSTEMS
        #include "./TinyVk_TimedGuard.hpp"
        #include "./TinyVk_Invokable.hpp"
        #include "./TinyVk_Utilities.hpp"
        #include "./TinyVk_Disposable.hpp"
    #pragma endregion
    #pragma region WINDOW_INPUT_HANDLING
        #include "./TinyVk_InputEnums.hpp"
        #include "./TinyVk_Window.hpp"
    #pragma endregion
    #pragma region VULKAN_INITIALIZATION
        #include "./TinyVk_VulkanDevice.hpp"
        #include "./TinyVk_CommandPool.hpp"
        #include "./TinyVk_GraphicsPipeline.hpp"
    #pragma endregion
    #pragma region TINYVULKAN_RENDERING
        #include "./TinyVk_RenderContext.hpp"
        #include "./TinyVk_Buffer.hpp"
        #include "./TinyVk_Image.hpp"
        #include "./TinyVk_GraphicsRenderer.hpp"
        #include "./TinyVk_SwapchainRenderer.hpp"
        #include "./TinyVk_ResourceQueue.hpp"
        #include "./TinyVk_VertexMath.hpp"
    #pragma endregion
#endif

/*
    TODO:
        Re-Create TinyVkSwapChainRenderer to extend TinyVkImageRenderer (avoiding duplicate code).
        Write and finalize TinyVkComputePipeline and TinyVkComputeRenderer.
            * Compute Pipeline should allow for both storage buffers & storage images.
    
    SYNCHRONIZATION?
        
*/