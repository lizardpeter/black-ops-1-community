#include <windows.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
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

    void Initialize(const Window &window)
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
        PickDevice();

        constexpr float priority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;
        const char *swapchainExtension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
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

    void PickDevice()
    {
        for (auto candidate : Enumerate<VkPhysicalDevice>(
                 [&](auto count, auto values) { return vkEnumeratePhysicalDevices(instance, count, values); }))
        {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(candidate, &properties);
            if (properties.apiVersion < VK_API_VERSION_1_4)
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
                    family = index;
                    std::printf("Vulkan device: %s\n", properties.deviceName);
                    return;
                }
            }
        }
        throw std::runtime_error("No Vulkan 1.4 graphics queue supports Win32 presentation");
    }
};

struct Swapchain
{
    const Vulkan &vk;
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkSemaphore> presentReady;

    explicit Swapchain(const Vulkan &vulkan) : vk(vulkan) {}
    ~Swapchain() { Reset(); }

    void Reset()
    {
        if (!handle)
            return;
        vkDeviceWaitIdle(vk.device);
        for (auto semaphore : presentReady)
            vkDestroySemaphore(vk.device, semaphore, nullptr);
        presentReady.clear();
        images.clear();
        vkDestroySwapchainKHR(vk.device, handle, nullptr);
        handle = VK_NULL_HANDLE;
    }

    void Create(HWND window, VkExtent2D clientExtent)
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.physical, vk.surface, &capabilities),
              "Query surface capabilities");
        if (!(capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
            throw std::runtime_error("The surface cannot clear swapchain images with a transfer");

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
        info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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

        Record(images[index]);
        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
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

    void Record(VkImage image)
    {
        Check(vkResetCommandBuffer(vk.command, 0), "Reset frame commands");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        Check(vkBeginCommandBuffer(vk.command, &begin), "Begin frame commands");

        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(vk.command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        const VkClearColorValue color{{0.024f, 0.037f, 0.055f, 1.0f}};
        vkCmdClearColorImage(vk.command, image, barrier.newLayout, &color, 1, &barrier.subresourceRange);

        barrier.oldLayout = barrier.newLayout;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = 0;
        vkCmdPipelineBarrier(vk.command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        Check(vkEndCommandBuffer(vk.command), "End frame commands");
    }
};
}

int main()
{
    try
    {
        const Window window;
        Vulkan vk;
        vk.Initialize(window);
        Swapchain swapchain(vk);
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
