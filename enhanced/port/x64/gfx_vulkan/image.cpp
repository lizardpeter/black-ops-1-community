#include <windows.h>
#include "image.h"
#include "image_vert.h"
#include "image_frag.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>
#include <stdexcept>
#include <string>

namespace bo1
{
namespace
{
void Check(VkResult result, const char *operation)
{
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + ": " + std::to_string(result));
}

VkFormat Format(IwiFormat format)
{
    switch (format)
    {
    case IwiFormat::bgra8: return VK_FORMAT_B8G8R8A8_SRGB;
    case IwiFormat::bc1: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    case IwiFormat::bc2: return VK_FORMAT_BC2_SRGB_BLOCK;
    case IwiFormat::bc3: return VK_FORMAT_BC3_SRGB_BLOCK;
    }
    throw std::runtime_error("Invalid IWI image format");
}

VkDeviceMemory Allocate(VkPhysicalDevice physical, VkDevice device, VkMemoryRequirements requirements,
                        VkMemoryPropertyFlags flags)
{
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physical, &properties);
    for (uint32_t index = 0; index < properties.memoryTypeCount; ++index)
    {
        if ((requirements.memoryTypeBits & (1u << index)) &&
            (properties.memoryTypes[index].propertyFlags & flags) == flags)
        {
            VkMemoryAllocateInfo info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            info.allocationSize = requirements.size;
            info.memoryTypeIndex = index;
            VkDeviceMemory memory = VK_NULL_HANDLE;
            Check(vkAllocateMemory(device, &info, nullptr, &memory), "Allocate texture memory");
            return memory;
        }
    }
    throw std::runtime_error("No suitable Vulkan memory type");
}

struct UploadResources
{
    VkDevice device;
    VkCommandPool pool;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkCommandBuffer command = VK_NULL_HANDLE;

    ~UploadResources()
    {
        if (command)
            vkFreeCommandBuffers(device, pool, 1, &command);
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
    }
};

struct ShaderModule
{
    VkDevice device;
    VkShaderModule module = VK_NULL_HANDLE;

    ShaderModule(VkDevice device, const uint32_t *code, std::size_t size) : device(device)
    {
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = size;
        info.pCode = code;
        Check(vkCreateShaderModule(device, &info, nullptr, &module), "Create image shader");
    }
    ~ShaderModule() { vkDestroyShaderModule(device, module, nullptr); }
};
}

VulkanImage::~VulkanImage()
{
    vkDestroyPipelineLayout(device, layout, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, emptyLayout, nullptr);
    vkDestroyBuffer(device, vertices, nullptr);
    vkFreeMemory(device, vertexMemory, nullptr);
    vkDestroySampler(device, sampler, nullptr);
    vkDestroyImageView(device, view, nullptr);
    vkDestroyImage(device, image, nullptr);
    vkFreeMemory(device, memory, nullptr);
}

void VulkanImage::Upload(VkPhysicalDevice physical, VkCommandPool pool, VkQueue queue, const IwiImage &source,
                         PreviewShaders sourceShaders)
{
    shaders = std::move(sourceShaders);
    const auto format = Format(source.format);
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(physical, format, &properties);
    constexpr auto needed = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if ((properties.optimalTilingFeatures & needed) != needed)
        throw std::runtime_error("The GPU cannot sample or upload this IWI format");
    dimensions = {source.width, source.height};

    UploadResources upload{device, pool};
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = source.topMip.size();
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    Check(vkCreateBuffer(device, &bufferInfo, nullptr, &upload.buffer), "Create texture upload buffer");
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, upload.buffer, &requirements);
    upload.memory = Allocate(physical, device, requirements,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    Check(vkBindBufferMemory(device, upload.buffer, upload.memory, 0), "Bind texture upload memory");
    void *mapped = nullptr;
    Check(vkMapMemory(device, upload.memory, 0, bufferInfo.size, 0, &mapped), "Map texture upload buffer");
    std::memcpy(mapped, source.topMip.data(), source.topMip.size());
    vkUnmapMemory(device, upload.memory);

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {source.width, source.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    Check(vkCreateImage(device, &imageInfo, nullptr, &image), "Create sampled texture");
    vkGetImageMemoryRequirements(device, image, &requirements);
    memory = Allocate(physical, device, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Check(vkBindImageMemory(device, image, memory, 0), "Bind sampled texture memory");

    VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate.commandPool = pool;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1;
    Check(vkAllocateCommandBuffers(device, &allocate, &upload.command), "Allocate texture upload commands");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    Check(vkBeginCommandBuffer(upload.command, &begin), "Begin texture upload commands");

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(upload.command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = imageInfo.extent;
    vkCmdCopyBufferToImage(upload.command, upload.buffer, image, barrier.newLayout, 1, &copy);
    barrier.oldLayout = barrier.newLayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(upload.command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    Check(vkEndCommandBuffer(upload.command), "End texture upload commands");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &upload.command;
    Check(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE), "Submit texture upload");
    Check(vkQueueWaitIdle(queue), "Wait for texture upload");

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = barrier.subresourceRange;
    Check(vkCreateImageView(device, &viewInfo, nullptr, &view), "Create sampled image view");

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
        samplerInfo.minFilter = samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW =
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    Check(vkCreateSampler(device, &samplerInfo, nullptr, &sampler), "Create image sampler");

    VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                         VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo descriptorInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    descriptorInfo.bindingCount = 1;
    descriptorInfo.pBindings = &binding;
    Check(vkCreateDescriptorSetLayout(device, &descriptorInfo, nullptr, &descriptorLayout),
          "Create image descriptor layout");
    VkDescriptorPoolSize size{binding.descriptorType, 1};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &size;
    Check(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool), "Create image descriptor pool");
    VkDescriptorSetAllocateInfo descriptorAllocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    descriptorAllocate.descriptorPool = descriptorPool;
    descriptorAllocate.descriptorSetCount = 1;
    descriptorAllocate.pSetLayouts = &descriptorLayout;
    Check(vkAllocateDescriptorSets(device, &descriptorAllocate, &descriptor), "Allocate image descriptor");
    VkDescriptorImageInfo sampled{sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = descriptor;
    write.descriptorCount = 1;
    write.descriptorType = binding.descriptorType;
    write.pImageInfo = &sampled;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorLayout;
    std::array<VkDescriptorSetLayout, 3> translatedLayouts{};
    if (!shaders.vertex.empty())
    {
        VkDescriptorSetLayoutCreateInfo empty{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        Check(vkCreateDescriptorSetLayout(device, &empty, nullptr, &emptyLayout), "Create empty shader layout");
        translatedLayouts = {emptyLayout, emptyLayout, descriptorLayout};
        layoutInfo.setLayoutCount = static_cast<uint32_t>(translatedLayouts.size());
        layoutInfo.pSetLayouts = translatedLayouts.data();

        constexpr float triangle[][8] = {
            {-1, -1, 0, 1, 0, 0, 0, 1},
            { 3, -1, 0, 1, 2, 0, 0, 1},
            {-1,  3, 0, 1, 0, 2, 0, 1},
        };
        VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buffer.size = sizeof(triangle);
        buffer.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        Check(vkCreateBuffer(device, &buffer, nullptr, &vertices), "Create preview vertex buffer");
        vkGetBufferMemoryRequirements(device, vertices, &requirements);
        vertexMemory = Allocate(physical, device, requirements,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        Check(vkBindBufferMemory(device, vertices, vertexMemory, 0), "Bind preview vertex buffer");
        void *vertexData = nullptr;
        Check(vkMapMemory(device, vertexMemory, 0, buffer.size, 0, &vertexData), "Map preview vertex buffer");
        std::memcpy(vertexData, triangle, sizeof(triangle));
        vkUnmapMemory(device, vertexMemory);
    }
    Check(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout), "Create image pipeline layout");
}

VkPipeline VulkanImage::CreatePipeline(VkFormat outputFormat) const
{
    const bool translated = !shaders.vertex.empty();
    const ShaderModule vertex(device, translated ? shaders.vertex.data() : image_vert,
                              translated ? shaders.vertex.size() * sizeof(uint32_t) : sizeof(image_vert));
    const ShaderModule fragment(device, translated ? shaders.fragment.data() : image_frag,
                                translated ? shaders.fragment.size() * sizeof(uint32_t) : sizeof(image_frag));
    std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                 VK_SHADER_STAGE_VERTEX_BIT, vertex.module, "main", nullptr};
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                 VK_SHADER_STAGE_FRAGMENT_BIT, fragment.module, "main", nullptr};

    VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    const VkVertexInputBindingDescription binding{0, 8 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    const std::array<VkVertexInputAttributeDescription, 2> attributes{{
        {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 4 * sizeof(float)},
    }};
    if (translated)
    {
        input.vertexBindingDescriptionCount = 1;
        input.pVertexBindingDescriptions = &binding;
        input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
        input.pVertexAttributeDescriptions = attributes.data();
    }
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments = &attachment;
    const std::array states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = static_cast<uint32_t>(states.size());
    dynamic.pDynamicStates = states.data();
    VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = &outputFormat;
    VkGraphicsPipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipeline.pNext = &rendering;
    pipeline.stageCount = static_cast<uint32_t>(stages.size());
    pipeline.pStages = stages.data();
    pipeline.pVertexInputState = &input;
    pipeline.pInputAssemblyState = &assembly;
    pipeline.pViewportState = &viewport;
    pipeline.pRasterizationState = &raster;
    pipeline.pMultisampleState = &samples;
    pipeline.pColorBlendState = &blend;
    pipeline.pDynamicState = &dynamic;
    pipeline.layout = layout;
    VkPipeline handle = VK_NULL_HANDLE;
    Check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline, nullptr, &handle),
          "Create image graphics pipeline");
    return handle;
}

void VulkanImage::Draw(VkCommandBuffer command, VkPipeline pipeline, VkExtent2D extent) const
{
    const float scale = std::min(float(extent.width) / dimensions.width, float(extent.height) / dimensions.height);
    const float width = dimensions.width * scale;
    const float height = dimensions.height * scale;
    const VkViewport viewport{(extent.width - width) * 0.5f, (extent.height - height) * 0.5f,
                              width, height, 0.0f, 1.0f};
    const VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(command, 0, 1, &viewport);
    vkCmdSetScissor(command, 0, 1, &scissor);
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    if (vertices)
    {
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(command, 0, 1, &vertices, &offset);
    }
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, vertices ? 2 : 0,
                            1, &descriptor, 0, nullptr);
    vkCmdDraw(command, 3, 1, 0, 0);
}
}
