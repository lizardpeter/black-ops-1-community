#pragma once

#include "../iwi.h"
#include "preview_shaders.h"
#include <vulkan/vulkan.h>

namespace bo1
{
class VulkanImage
{
public:
    explicit VulkanImage(VkDevice device) : device(device) {}
    ~VulkanImage();
    VulkanImage(const VulkanImage &) = delete;
    VulkanImage &operator=(const VulkanImage &) = delete;

    void Upload(VkPhysicalDevice physical, VkCommandPool pool, VkQueue queue, const IwiImage &source, PreviewShaders shaders);
    VkPipeline CreatePipeline(VkFormat outputFormat) const;
    void Draw(VkCommandBuffer command, VkPipeline pipeline, VkExtent2D extent) const;
    bool IsLoaded() const { return image != VK_NULL_HANDLE; }

private:
    VkDevice device;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptor = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkExtent2D dimensions{};
    PreviewShaders shaders;
    VkDescriptorSetLayout emptyLayout = VK_NULL_HANDLE;
    VkBuffer vertices = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
};
}
