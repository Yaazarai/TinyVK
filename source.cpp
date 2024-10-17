#define TINYVK_ALLOWS_POLLING_GAMEPADS
#include "./TinyVulkan/TinyVulkan.hpp"
using namespace tinyvk;

//#define DEFAULT_COMPUTE_SHADER "./Shaders/passthrough_comp.spv"
#define DEFAULT_VERTEX_SHADER "./Shaders/passthrough_vert.spv"
#define DEFAULT_FRAGMENT_SHADER "./Shaders/passthrough_frag.spv"

//const std::string computeShader = DEFAULT_COMPUTE_SHADER;
const std::tuple<VkShaderStageFlagBits, std::string> vertexShader = { VK_SHADER_STAGE_VERTEX_BIT, DEFAULT_VERTEX_SHADER };
const std::tuple<VkShaderStageFlagBits, std::string> fragmentShader = { VK_SHADER_STAGE_FRAGMENT_BIT, DEFAULT_FRAGMENT_SHADER };
const std::vector<std::tuple<VkShaderStageFlagBits, std::string>> defaultShaders = { vertexShader, fragmentShader };

const std::vector<VkPhysicalDeviceType> rdeviceTypes = { VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU };
const TinyVkBufferingMode bufferingMode = TinyVkBufferingMode::DOUBLE;
const TinyVkVertexDescription vertexDescription = TinyVkVertex::GetVertexDescription();
const std::vector<VkDescriptorSetLayoutBinding> pushDescriptorLayouts = { TinyVkGraphicsPipeline::SelectPushDescriptorLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1) };

int32_t TINYVULKAN_WINDOWMAIN {
    TinyVkWindow window("Sample Application", 1920, 1080, true, false);
    TinyVkVulkanDevice vkdevice("Sample Application", false, rdeviceTypes, &window);    
    TinyVkCommandPool commandPool(vkdevice, false);

    TinyVkGraphicsPipeline pipeline(vkdevice, vertexDescription, defaultShaders, pushDescriptorLayouts, {}, false);
    TinyVkRenderContext renderContext(vkdevice, commandPool, pipeline);
    TinyVkSwapchainRenderer swapRenderer(renderContext, window, bufferingMode, TinyVkCommandPool::defaultCommandPoolSize);
    //TinyVkComputeRender computeRenderer(renderContext, computeVertexDescription, computeDefaultShaders, computePushDescriptorLayouts, {}, false);

    std::vector<TinyVkVertex> triangles = {
        TinyVkVertex({0.0f,0.0f}, {240.0f,135.0f,               1.0f}, {1.0f,0.0f,0.0f,1.0f}),
        TinyVkVertex({0.0f,0.0f}, {240.0f+960.0f,135.0f,        1.0f}, {0.0f,1.0f,0.0f,1.0f}),
        TinyVkVertex({0.0f,0.0f}, {240.0f+960.0f,135.0f+540.0f, 1.0f}, {1.0f,0.0f,1.0f,1.0f}),
        TinyVkVertex({0.0f,0.0f}, {240.0f,135.0f + 540.0f,      1.0f}, {0.0f,0.0f,1.0f,1.0f})
    };
    //std::vector<TinyVkVertex> triangles = TinyVkQuad::CreateWithOffsetExt(glm::vec2(960.0f/2.0, 540.0f/2.0), glm::vec3(960.0f, 540.0f, 0.0f));
    std::vector<uint32_t> indices = {0,1,2,2,3,0};

    size_t sizeofTriangles = TinyVkBuffer::GetSizeofVector<TinyVkVertex>(triangles);
    TinyVkBuffer vbuffer(renderContext, sizeofTriangles, TinyVkBufferType::TINYVK_BUFFER_TYPE_VERTEX);
    vbuffer.StageBufferData(triangles.data(), sizeofTriangles, 0, 0);
    
    size_t sizeofIndices = TinyVkBuffer::GetSizeofVector<uint32_t>(indices);
    TinyVkBuffer ibuffer(renderContext, sizeofIndices, TinyVkBufferType::TINYVK_BUFFER_TYPE_INDEX);
    ibuffer.StageBufferData(indices.data(), sizeofIndices, 0, 0);
    
    TinyVkBuffer projection1(renderContext, sizeof(glm::mat4), TinyVkBufferType::TINYVK_BUFFER_TYPE_UNIFORM);
    TinyVkBuffer projection2(renderContext, sizeof(glm::mat4), TinyVkBufferType::TINYVK_BUFFER_TYPE_UNIFORM);

    struct SwapFrame {
        TinyVkBuffer &projection;
        SwapFrame(TinyVkBuffer& projection) : projection(projection) {}
    };

    TinyVkResourceQueue<SwapFrame,static_cast<size_t>(bufferingMode)> queue(
        {
            SwapFrame(projection1),
            SwapFrame(projection2)
        },
        TinyVkCallback<size_t&>([&swapRenderer](size_t& frameIndex){ frameIndex = swapRenderer.GetSyncronizedFrameIndex(); }),
        TinyVkCallback<SwapFrame&>([](SwapFrame& resource){})
    );
    
    /// TESTING RENDERCONTEXT CHANGES WITH THE IMAGE RENDERER.
    TinyVkImage sourceImage(renderContext, TinyVkImageType::TINYVK_IMAGE_TYPE_COLORATTACHMENT, 960, 540);
    TinyVkGraphicsRenderer imageRenderer(renderContext, &commandPool, &sourceImage, VK_NULL_HANDLE);
    
    imageRenderer.onRenderEvents.hook(TinyVkCallback<TinyVkCommandPool&>(
    [&indices, &vkdevice, &window, &imageRenderer, &pipeline, &vbuffer, &ibuffer, &projection1](TinyVkCommandPool& commandPool) {
        glm::mat4 camera = TinyVkMath::Project2D(window.hwndWidth, window.hwndHeight, 0, 0, 1.0, 0.0);
        
        projection1.StageBufferData(&camera, sizeof(glm::mat4), 0, 0);
        auto commandBuffer = commandPool.LeaseBuffer(false);

        imageRenderer.BeginRecordCmdBuffer(commandBuffer.first /*, clearColor, depthStencil*/);
            VkDescriptorBufferInfo cameraDescriptorInfo = projection1.GetBufferDescriptor();
            VkWriteDescriptorSet cameraDescriptor = pipeline.SelectWriteBufferDescriptor(0, 1, { &cameraDescriptorInfo });
            imageRenderer.PushDescriptorSet(commandBuffer.first, { cameraDescriptor });
            
            VkDeviceSize offsets[] = { 0 };
            imageRenderer.CmdBindGeometry(commandBuffer.first, &vbuffer.buffer, ibuffer.buffer, offsets);
            imageRenderer.CmdDrawGeometry(commandBuffer.first, true, 1, 0, indices.size(), 0, 0);    
        imageRenderer.EndRecordCmdBuffer(commandBuffer.first /*, clearColor, depthStencil*/);
    }));
    imageRenderer.RenderExecute();
    
    VkClearValue clearColor{ .color.float32 = { 0.0, 0.0, 0.5, 1.0 } };
    VkClearValue depthStencil{ .depthStencil = { 1.0f, 0 } };

    // TESTING RENDERCONTEXT CHANGES WITH THE SWAPCHAIN RENDERER.
    int angle = 0;
    swapRenderer.onRenderEvents.hook(TinyVkCallback<TinyVkCommandPool&>(
        [&angle, &indices, &vkdevice, &window, &swapRenderer, &pipeline, &queue, &vbuffer, &ibuffer, &clearColor](TinyVkCommandPool& commandPool) {
        auto frame = queue.GetFrameResource();

        auto commandBuffer = commandPool.LeaseBuffer();
        swapRenderer.BeginRecordCmdBuffer(commandBuffer.first);
        
        int offsetx = 0, offsety = 0;
        offsetx = glm::sin(glm::radians(static_cast<glm::float32>(angle))) * 64;
        offsety = glm::cos(glm::radians(static_cast<glm::float32>(angle))) * 64;

        //offsetx = glfwGetGamepadAxis(TinyVkGamepads::GPAD_01,TinyVkGamepadAxis::AXIS_LEFTX) * 64;
        //offsety = glfwGetGamepadAxis(TinyVkGamepads::GPAD_01,TinyVkGamepadAxis::AXIS_LEFTY) * 64;

        //double mousex, mousey;
        //glfwGetCursorPos(window.GetHandle(), &mousex, &mousey);
        //int leftclick = glfwGetMouseButton(window.GetHandle(), GLFW_MOUSE_BUTTON_1);
        //offsetx = int(mousex) * leftclick;
        //offsety = int(mousey) * leftclick;

        glm::mat4 camera = TinyVkMath::Project2D(window.hwndWidth, window.hwndHeight, offsetx, offsety, 1.0, 0.0);
        frame.projection.StageBufferData(&camera, sizeof(glm::mat4), 0, 0);
        VkDescriptorBufferInfo cameraDescriptorInfo = frame.projection.GetBufferDescriptor();
        VkWriteDescriptorSet cameraDescriptor = pipeline.SelectWriteBufferDescriptor(0, 1, { &cameraDescriptorInfo });
        swapRenderer.PushDescriptorSet(commandBuffer.first, { cameraDescriptor });
        
        VkDeviceSize offsets[] = { 0 };
        swapRenderer.CmdBindGeometry(commandBuffer.first, &vbuffer.buffer, ibuffer.buffer, offsets);
        swapRenderer.CmdDrawGeometry(commandBuffer.first, true, 1, 0, indices.size(), 0, 0);    
        swapRenderer.EndRecordCmdBuffer(commandBuffer.first);
        
        angle += 1;

        //if (angle % 200 == 0) swapRenderer.PushPresentMode(VK_PRESENT_MODE_IMMEDIATE_KHR);
        //if (angle % 400 == 0) swapRenderer.PushPresentMode(VK_PRESENT_MODE_FIFO_KHR);
    }));

    // MULTI-THREADED: (window events on main, rendering on secondary)
    std::thread mythread([&window, &swapRenderer]() { while (!window.ShouldClose()) { swapRenderer.RenderExecute(); } });
    window.WhileMain(true);
    mythread.join();

    // SINGLE-THREADED: (window/render events within the same thread, window events will block rendering temporarily)
    //window.onWhileMain.hook(TinyVkCallback<std::atomic_bool&>([&swapRenderer](std::atomic_bool& check){ swapRenderer.RenderSwapChain(); }));
    //window.WhileMain(false);
    return VK_SUCCESS;
}