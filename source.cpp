#include "./TinyVulkan/TinyVulkan.hpp"
using namespace tinyvk;

#define DEFAULT_VERTEX_SHADER "./Shaders/passthrough_vert.spv"
#define DEFAULT_FRAGMENT_SHADER "./Shaders/passthrough_frag.spv"
const std::tuple<VkShaderStageFlagBits, std::string> vertexShader = { VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, DEFAULT_VERTEX_SHADER };
const std::tuple<VkShaderStageFlagBits, std::string> fragmentShader = { VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT, DEFAULT_FRAGMENT_SHADER };
const std::vector<std::tuple<VkShaderStageFlagBits, std::string>> defaultShaders = { vertexShader, fragmentShader };

const std::vector<VkPhysicalDeviceType> rdeviceTypes = { VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU };
const TinyVkBufferingMode bufferingMode = TinyVkBufferingMode::DOUBLE;
const TinyVkVertexDescription vertexDescription = TinyVkVertex::GetVertexDescription();
const std::vector<VkDescriptorSetLayoutBinding> pushDescriptorLayouts = { TinyVkGraphicsPipeline::SelectPushDescriptorLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1) };

#define DEFAULT_COMMAND_POOLSIZE 20

int32_t TINYVULKAN_WINDOWMAIN {
    TinyVkWindow window("Sample Application", 1440, 810, true, false);
    TinyVkVulkanDevice vkdevice("Sample Application", rdeviceTypes, &window, window.QueryRequiredExtensions(TINYVK_VALIDATION_LAYERS));
    TinyVkCommandPool commandPool(vkdevice, DEFAULT_COMMAND_POOLSIZE);
    TinyVkGraphicsPipeline pipeline(vkdevice, vertexDescription, defaultShaders, pushDescriptorLayouts, {}, false);
    TinyVkSwapChainRenderer swapRenderer(vkdevice, window, pipeline, bufferingMode);
    
    //VkClearValue clearColor { .color = { 0.0, 0.0, 0.0, 1.0f } };
    //VkClearValue depthStencil { .depthStencil = { 1.0f, 0 } };

    std::vector<TinyVkVertex> triangles = TinyVkQuad::CreateWithOffsetExt(glm::vec2(240.0f, 135.0f), glm::vec3(960.0f, 540.0f, 0.0f));
    std::vector<uint32_t> indices = {0,1,2,2,3,0};

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
    
    int angle = 0;
    swapRenderer.onRenderEvents.hook(TinyVkCallback<TinyVkCommandPool&>([&angle, &vkdevice, &window, &swapRenderer, &pipeline, &queue /*, &clearColor, &depthStencil*/](TinyVkCommandPool& commandPool) {
        auto frame = queue.GetFrameResource();

        auto commandBuffer = commandPool.LeaseBuffer();
        swapRenderer.BeginRecordCmdBuffer(commandBuffer.first /*, clearColor, depthStencil*/);
        
        int offsetx, offsety;
        offsetx = glm::sin(glm::radians(static_cast<glm::float32>(angle))) * 64;
        offsety = glm::cos(glm::radians(static_cast<glm::float32>(angle))) * 64;
        glm::mat4 camera = TinyVkMath::Project2D(window.GetWidth(), window.GetHeight(), offsetx, offsety, 1.0, 0.0);
        frame.projection.StageBufferData(&camera, sizeof(glm::mat4), 0, 0);
        VkDescriptorBufferInfo cameraDescriptorInfo = frame.projection.GetBufferDescriptor();
        VkWriteDescriptorSet cameraDescriptor = pipeline.SelectWriteBufferDescriptor(0, 1, { &cameraDescriptorInfo });
        swapRenderer.PushDescriptorSet(commandBuffer.first, { cameraDescriptor });
        
        VkDeviceSize offsets[] = { 0 };
        swapRenderer.CmdBindGeometry(commandBuffer.first, &frame.vbuffer.buffer, frame.ibuffer.buffer, offsets);
        swapRenderer.CmdDrawGeometry(commandBuffer.first, true, 1, 0, 6, 0, 0);
        swapRenderer.EndRecordCmdBuffer(commandBuffer.first /*, clearColor, depthStencil*/);

        angle += 1.25;
    }));

    std::thread mythread([&window, &swapRenderer]() { while (!window.ShouldClose()) { swapRenderer.RenderExecute(); } });
    window.WhileMain(true);
    mythread.join();
    return VK_SUCCESS;
}