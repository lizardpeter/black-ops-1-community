#include <windows.h>
#include <vulkan/vulkan.h>

#include "image.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
void Check(VkResult result, const char *operation)
{
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + " failed: " + std::to_string(result));
}

template <typename T, typename Query>
std::vector<T> Enumerate(Query query)
{
    for (;;)
    {
        uint32_t count = 0;
        Check(query(&count, nullptr), "Vulkan enumeration count");
        std::vector<T> values(count);
        if (count == 0)
            return values;
        const auto result = query(&count, values.data());
        if (result == VK_INCOMPLETE)
            continue;
        Check(result, "Vulkan enumeration");
        values.resize(count);
        return values;
    }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CLOSE)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

struct Window
{
    HINSTANCE module = GetModuleHandleW(nullptr);
    HWND handle = nullptr;

    Window()
    {
        WNDCLASSW wc{};
        wc.hInstance = module;
        wc.lpfnWndProc = WindowProc;
        wc.lpszClassName = L"BO1VulkanBringup";
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        if (!RegisterClassW(&wc))
            throw std::runtime_error("RegisterClassW failed");
        handle = CreateWindowExW(0, wc.lpszClassName, L"BO1 Vulkan bring-up", WS_OVERLAPPEDWINDOW,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 960, 540,
                                 nullptr, nullptr, module, nullptr);
        if (!handle)
            throw std::runtime_error("CreateWindowExW failed");
        ShowWindow(handle, SW_SHOW);
    }

    ~Window() { DestroyWindow(handle); }
};

struct Vulkan
{
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    uint32_t family = 0;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    bool supportsBc = false;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer command = VK_NULL_HANDLE;
    VkFence frameDone = VK_NULL_HANDLE;
    VkSemaphore imageReady = VK_NULL_HANDLE;

    ~Vulkan()
    {
        if (device)
        {
            vkDeviceWaitIdle(device);
            vkDestroySemaphore(device, imageReady, nullptr);
            vkDestroyFence(device, frameDone, nullptr);
            vkDestroyCommandPool(device, pool, nullptr);
            vkDestroyDevice(device, nullptr);
        }
        if (surface)
            vkDestroySurfaceKHR(instance, surface, nullptr);
        if (instance)
            vkDestroyInstance(instance, nullptr);
    }

    void Initialize(const Window &window, bool needsBc)
    {
        const std::array extensions{VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
        VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        application.pApplicationName = "BO1 Vulkan bring-up";
        application.apiVersion = VK_API_VERSION_1_4;
        VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        create.pApplicationInfo = &application;
        create.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        create.ppEnabledExtensionNames = extensions.data();
        Check(vkCreateInstance(&create, nullptr, &instance), "Create Vulkan 1.4 instance");

        VkWin32SurfaceCreateInfoKHR surfaceInfo{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
        surfaceInfo.hinstance = window.module;
        surfaceInfo.hwnd = window.handle;
        Check(vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface), "Create Win32 surface");
        PickDevice(needsBc);

        constexpr float priority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;
        const char *swapchainExtension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        VkPhysicalDeviceVulkan13Features rendering{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        rendering.dynamicRendering = VK_TRUE;
        VkPhysicalDeviceFeatures features{};
        features.textureCompressionBC = supportsBc;
        VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        deviceInfo.pNext = &rendering;
        deviceInfo.pEnabledFeatures = &features;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = 1;
        deviceInfo.ppEnabledExtensionNames = &swapchainExtension;
        Check(vkCreateDevice(physical, &deviceInfo, nullptr, &device), "Create Vulkan device");
        vkGetDeviceQueue(device, family, 0, &queue);

        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.queueFamilyIndex = family;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        Check(vkCreateCommandPool(device, &poolInfo, nullptr, &pool), "Create command pool");
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool = pool;
        allocate.commandBufferCount = 1;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        Check(vkAllocateCommandBuffers(device, &allocate, &command), "Allocate command buffer");

        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        Check(vkCreateFence(device, &fenceInfo, nullptr, &frameDone), "Create frame fence");
        VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        Check(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageReady), "Create acquire semaphore");
    }

    void PickDevice(bool needsBc)
    {
        for (auto candidate : Enumerate<VkPhysicalDevice>(
                 [&](auto count, auto values) { return vkEnumeratePhysicalDevices(instance, count, values); }))
        {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(candidate, &properties);
            if (properties.apiVersion < VK_API_VERSION_1_4)
                continue;

            VkPhysicalDeviceVulkan13Features rendering{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
            VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
            features.pNext = &rendering;
            vkGetPhysicalDeviceFeatures2(candidate, &features);
            if (!rendering.dynamicRendering || (needsBc && !features.features.textureCompressionBC))
                continue;

            const auto extensions = Enumerate<VkExtensionProperties>([&](auto count, auto values) {
                return vkEnumerateDeviceExtensionProperties(candidate, nullptr, count, values);
            });
            const auto swapchain = std::find_if(extensions.begin(), extensions.end(), [](const auto &extension) {
                return std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
            });
            if (swapchain == extensions.end())
                continue;

            uint32_t count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, families.data());
            for (uint32_t index = 0; index < count; ++index)
            {
                VkBool32 present = VK_FALSE;
                Check(vkGetPhysicalDeviceSurfaceSupportKHR(candidate, index, surface, &present),
                      "Query present support");
                if (present && families[index].queueCount && (families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                {
                    physical = candidate;
                    supportsBc = features.features.textureCompressionBC;
                    family = index;
                    std::printf("Vulkan device: %s\n", properties.deviceName);
                    return;
                }
            }
        }
        throw std::runtime_error("No Vulkan 1.4 graphics queue supports the required presentation and texture features");
    }
};

struct Swapchain
{
    const Vulkan &vk;
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    const bo1::VulkanImage &texture;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkExtent2D imageExtent{};
    std::vector<VkSemaphore> presentReady;

    Swapchain(const Vulkan &vulkan, const bo1::VulkanImage &source) : vk(vulkan), texture(source) {}
    ~Swapchain() { Reset(); }

    void Reset()
    {
        if (!handle)
            return;
        vkDeviceWaitIdle(vk.device);
        for (auto semaphore : presentReady)
            vkDestroySemaphore(vk.device, semaphore, nullptr);
        presentReady.clear();
        for (auto view : views)
            vkDestroyImageView(vk.device, view, nullptr);
        views.clear();
        vkDestroyPipeline(vk.device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
        images.clear();
        vkDestroySwapchainKHR(vk.device, handle, nullptr);
        handle = VK_NULL_HANDLE;
    }

    void Create(HWND window, VkExtent2D clientExtent)
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.physical, vk.surface, &capabilities),
              "Query surface capabilities");
        if (!(capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
            throw std::runtime_error("The surface does not support swapchain color attachments");

        const auto formats = Enumerate<VkSurfaceFormatKHR>([&](auto count, auto values) {
            return vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physical, vk.surface, count, values);
        });
        if (formats.empty())
            throw std::runtime_error("The surface has no formats");
        auto format = formats.front();
        for (const auto &candidate : formats)
        {
            if (candidate.format == VK_FORMAT_B8G8R8A8_SRGB &&
                candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                format = candidate;
        }
        if (format.format == VK_FORMAT_UNDEFINED)
            format.format = VK_FORMAT_B8G8R8A8_UNORM;

        const auto modes = Enumerate<VkPresentModeKHR>([&](auto count, auto values) {
            return vkGetPhysicalDeviceSurfacePresentModesKHR(vk.physical, vk.surface, count, values);
        });
        const bool immediate = std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end();
        VkExtent2D extent = capabilities.currentExtent;
        if (extent.width == UINT32_MAX)
        {
            extent.width = std::clamp(clientExtent.width, capabilities.minImageExtent.width,
                                      capabilities.maxImageExtent.width);
            extent.height = std::clamp(clientExtent.height, capabilities.minImageExtent.height,
                                       capabilities.maxImageExtent.height);
        }
        if (!extent.width || !extent.height)
            return;

        VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        info.surface = vk.surface;
        info.minImageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount)
            info.minImageCount = std::min(info.minImageCount, capabilities.maxImageCount);
        info.imageFormat = format.format;
        info.imageColorSpace = format.colorSpace;
        info.imageExtent = extent;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = capabilities.currentTransform;
        for (auto alpha : {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                           VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
        {
            if (capabilities.supportedCompositeAlpha & alpha)
            {
                info.compositeAlpha = alpha;
                break;
            }
        }
        info.presentMode = immediate ? VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_FIFO_KHR;
        info.clipped = VK_TRUE;
        const auto created = vkCreateSwapchainKHR(vk.device, &info, nullptr, &handle);
        if (created == VK_ERROR_OUT_OF_DATE_KHR)
            return;
        Check(created, "Create swapchain");

        images = Enumerate<VkImage>([&](auto count, auto values) {
            return vkGetSwapchainImagesKHR(vk.device, handle, count, values);
        });
        imageExtent = extent;
        views.resize(images.size(), VK_NULL_HANDLE);
        for (std::size_t i = 0; i < images.size(); ++i)
        {
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = images[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = format.format;
            viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            Check(vkCreateImageView(vk.device, &viewInfo, nullptr, &views[i]), "Create swapchain image view");
        }
        if (texture.IsLoaded())
            pipeline = texture.CreatePipeline(format.format);
        presentReady.resize(images.size(), VK_NULL_HANDLE);
        VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        for (auto &semaphore : presentReady)
            Check(vkCreateSemaphore(vk.device, &semaphoreInfo, nullptr, &semaphore), "Create present semaphore");

        SetWindowTextW(window, immediate ? L"BO1 Vulkan 1.4 | IMMEDIATE" : L"BO1 Vulkan 1.4 | FIFO");
        std::printf("Swapchain: %u x %u, %zu images, %s\n", extent.width, extent.height,
                    images.size(), immediate ? "IMMEDIATE" : "FIFO fallback");
    }

    void Draw()
    {
        const auto fence = vkGetFenceStatus(vk.device, vk.frameDone);
        if (fence == VK_NOT_READY)
        {
            Sleep(1);
            return;
        }
        Check(fence, "Read frame fence");

        uint32_t index = 0;
        const auto acquired = vkAcquireNextImageKHR(vk.device, handle, 0, vk.imageReady, VK_NULL_HANDLE, &index);
        if (acquired == VK_NOT_READY || acquired == VK_TIMEOUT)
        {
            Sleep(1);
            return;
        }
        if (acquired == VK_ERROR_OUT_OF_DATE_KHR)
        {
            Reset();
            return;
        }
        if (acquired != VK_SUBOPTIMAL_KHR)
            Check(acquired, "Acquire swapchain image");

        Record(index);
        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &vk.imageReady;
        submit.pWaitDstStageMask = &waitStage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &vk.command;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &presentReady[index];
        Check(vkResetFences(vk.device, 1, &vk.frameDone), "Reset frame fence");
        Check(vkQueueSubmit(vk.queue, 1, &submit, vk.frameDone), "Submit frame");

        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &presentReady[index];
        present.swapchainCount = 1;
        present.pSwapchains = &handle;
        present.pImageIndices = &index;
        const auto result = vkQueuePresentKHR(vk.queue, &present);
        if (result != VK_ERROR_OUT_OF_DATE_KHR && result != VK_SUBOPTIMAL_KHR)
            Check(result, "Present frame");
        if (result != VK_SUCCESS || acquired == VK_SUBOPTIMAL_KHR)
            Reset();
    }

    void Record(uint32_t index)
    {
        Check(vkResetCommandBuffer(vk.command, 0), "Reset frame commands");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        Check(vkBeginCommandBuffer(vk.command, &begin), "Begin frame commands");

        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = images[index];
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        constexpr auto colorStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        vkCmdPipelineBarrier(vk.command, colorStage, colorStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        attachment.imageView = views[index];
        attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.clearValue.color = {{0.024f, 0.037f, 0.055f, 1.0f}};
        VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
        rendering.renderArea.extent = imageExtent;
        rendering.layerCount = 1;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachments = &attachment;
        vkCmdBeginRendering(vk.command, &rendering);
        if (pipeline)
            texture.Draw(vk.command, pipeline, imageExtent);
        vkCmdEndRendering(vk.command);

        barrier.oldLayout = barrier.newLayout;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        vkCmdPipelineBarrier(vk.command, colorStage, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        Check(vkEndCommandBuffer(vk.command), "End frame commands");
    }
};
}

int main(int argc, char **argv)
{
    try
    {
        if (argc > 2)
            throw std::runtime_error("Usage: bo1_vulkan_probe [path-to-image.iwi]");
        bo1::IwiImage source{};
        if (argc == 2)
        {
            std::ifstream file(argv[1], std::ios::binary);
            if (!file)
                throw std::runtime_error(std::string("Cannot open IWI: ") + argv[1]);
            const std::string bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
            source = bo1::DecodeIwi(bytes);
            std::printf("IWI: %u x %u\n", source.width, source.height);
        }

        const Window window;
        Vulkan vk;
        vk.Initialize(window, argc == 2 && source.format != bo1::IwiFormat::bgra8);
        bo1::VulkanImage texture(vk.device);
        if (argc == 2)
            texture.Upload(vk.physical, vk.pool, vk.queue, source);
        Swapchain swapchain(vk, texture);
        VkExtent2D previous{};
        for (;;)
        {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                if (message.message == WM_QUIT)
                    return 0;
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            RECT rect{};
            GetClientRect(window.handle, &rect);
            const VkExtent2D extent{static_cast<uint32_t>(rect.right), static_cast<uint32_t>(rect.bottom)};
            if (IsIconic(window.handle) || !extent.width || !extent.height)
            {
                WaitMessage();
                continue;
            }
            if (extent.width != previous.width || extent.height != previous.height)
            {
                swapchain.Reset();
                previous = extent;
            }
            if (!swapchain.handle)
                swapchain.Create(window.handle, extent);
            if (swapchain.handle)
                swapchain.Draw();
            else
                Sleep(1);
        }
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
