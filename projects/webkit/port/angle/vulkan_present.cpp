// The GPU compositor's frames handed to the window without a copy on the
// processor (PORT.md, "Compositing"): the composed frame is a VkImage of
// the port's on ANGLE's VkDevice (B8G8R8A8, the desktop's ARGB words in
// memory), which ANGLE renders into as an EGLImage
// (EGL_ANGLE_vulkan_image), and the window's buffer (a wl_shm pool's
// memory) is imported into the same device as host memory
// (VK_EXT_external_memory_host, which ANGLE enables where the driver has
// it): a copy on ANGLE's queue writes the frame into the buffer the
// Wayland compositor reads. MlxLayerTreeHost.cpp does the GL and EGL side
// (the image, glReleaseTexturesANGLE before the copy and
// glAcquireTexturesANGLE after, the queue's lock); this file, compiled
// with vulkan/vulkan.h (mlx-vulkan's) in libANGLE.a, the Vulkan side.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

namespace {

struct Presenter {
    VkPhysicalDevice physicalDevice { VK_NULL_HANDLE };
    VkDevice device { VK_NULL_HANDLE };
    VkQueue queue { VK_NULL_HANDLE };
    uint32_t family { 0 };
    VkDeviceSize hostAlignment { 4096 };
    PFN_vkGetMemoryHostPointerPropertiesEXT hostPointerProperties { nullptr };
    VkCommandPool pool { VK_NULL_HANDLE };
    VkCommandBuffer commands { VK_NULL_HANDLE };
    VkFence fence { VK_NULL_HANDLE };
    VkImage image { VK_NULL_HANDLE };
    VkDeviceMemory imageMemory { VK_NULL_HANDLE };
    VkImageCreateInfo imageInfo { };
};

uint32_t memoryType(Presenter& p, uint32_t bits, VkMemoryPropertyFlags wanted)
{
    VkPhysicalDeviceMemoryProperties properties;
    vkGetPhysicalDeviceMemoryProperties(p.physicalDevice, &properties);
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        if ((bits & (1u << i)) && (properties.memoryTypes[i].propertyFlags & wanted) == wanted)
            return i;
    }
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        if (bits & (1u << i))
            return i;
    }
    return UINT32_MAX;
}

void destroyImage(Presenter& p)
{
    if (p.image)
        vkDestroyImage(p.device, p.image, nullptr);
    if (p.imageMemory)
        vkFreeMemory(p.device, p.imageMemory, nullptr);
    p.image = VK_NULL_HANDLE;
    p.imageMemory = VK_NULL_HANDLE;
}

// The image layout GL_EXT_semaphore names (what glReleaseTexturesANGLE
// gives) as Vulkan's.
VkImageLayout vulkanLayout(uint32_t glLayout)
{
    switch (glLayout) {
    case 0x958D: return VK_IMAGE_LAYOUT_GENERAL;
    case 0x958E: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case 0x958F: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case 0x9590: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    case 0x9591: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case 0x9592: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case 0x9593: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    default: return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

} // namespace

// A presenter on ANGLE's device (its handles from EGL_ANGLE_device_vulkan),
// or null when the device cannot import host memory.
extern "C" void* mlxVulkanPresenterCreate(void* physicalDevice, void* device, void* queue, uint32_t family)
{
    auto* p = new Presenter;
    p->physicalDevice = static_cast<VkPhysicalDevice>(physicalDevice);
    p->device = static_cast<VkDevice>(device);
    p->queue = static_cast<VkQueue>(queue);
    p->family = family;
    p->hostPointerProperties = reinterpret_cast<PFN_vkGetMemoryHostPointerPropertiesEXT>(vkGetDeviceProcAddr(p->device, "vkGetMemoryHostPointerPropertiesEXT"));
    VkPhysicalDeviceExternalMemoryHostPropertiesEXT host { };
    host.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT;
    VkPhysicalDeviceProperties2 properties { };
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &host;
    vkGetPhysicalDeviceProperties2(p->physicalDevice, &properties);
    if (host.minImportedHostPointerAlignment)
        p->hostAlignment = host.minImportedHostPointerAlignment;
    VkCommandPoolCreateInfo poolInfo { };
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = family;
    VkFenceCreateInfo fenceInfo { };
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (!p->hostPointerProperties || vkCreateCommandPool(p->device, &poolInfo, nullptr, &p->pool) != VK_SUCCESS || vkCreateFence(p->device, &fenceInfo, nullptr, &p->fence) != VK_SUCCESS) {
        if (p->pool)
            vkDestroyCommandPool(p->device, p->pool, nullptr);
        delete p;
        return nullptr;
    }
    VkCommandBufferAllocateInfo allocateInfo { };
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = p->pool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(p->device, &allocateInfo, &p->commands);
    return p;
}

extern "C" void mlxVulkanPresenterDestroy(void* presenter)
{
    auto* p = static_cast<Presenter*>(presenter);
    if (!p)
        return;
    vkDeviceWaitIdle(p->device);
    destroyImage(*p);
    vkDestroyFence(p->device, p->fence, nullptr);
    vkDestroyCommandPool(p->device, p->pool, nullptr);
    delete p;
}

// The frame's image of WIDTH x HEIGHT (made anew, the last one gone): its
// VkImage into IMAGE and its VkImageCreateInfo into INFO, what
// eglCreateImageKHR's EGL_VULKAN_IMAGE_ANGLE takes; false when it cannot
// be made.
extern "C" bool mlxVulkanPresenterMakeImage(void* presenter, int width, int height, void** image, const void** info)
{
    auto& p = *static_cast<Presenter*>(presenter);
    destroyImage(p);
    VkImageCreateInfo& imageInfo = p.imageInfo;
    imageInfo = { };
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
    imageInfo.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(p.device, &imageInfo, nullptr, &p.image) != VK_SUCCESS) {
        p.image = VK_NULL_HANDLE;
        return false;
    }
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(p.device, p.image, &requirements);
    VkMemoryAllocateInfo allocateInfo { };
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = memoryType(p, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (allocateInfo.memoryTypeIndex == UINT32_MAX || vkAllocateMemory(p.device, &allocateInfo, nullptr, &p.imageMemory) != VK_SUCCESS || vkBindImageMemory(p.device, p.image, p.imageMemory, 0) != VK_SUCCESS) {
        destroyImage(p);
        return false;
    }
    *image = reinterpret_cast<void*>(p.image);
    *info = &p.imageInfo;
    return true;
}

// The image's top left WIDTH x HEIGHT (in GL_LAYOUT, a GL_EXT_semaphore
// layout, as glReleaseTexturesANGLE left it; the layout it is left in, for
// glAcquireTexturesANGLE, into LAYOUT_AFTER) into
// PIXELS (STRIDE words a row: the window's buffer, its memory imported for
// the copy); LOCK and UNLOCK around the queue's use (ANGLE's
// eglLockVulkanQueueANGLE). Waits for the copy; false when the memory
// cannot be imported or the copy fails.
extern "C" bool mlxVulkanPresenterCopy(void* presenter, uint32_t glLayout, uint32_t* layoutAfter, uint32_t* pixels, int width, int height, int stride, void (*lock)(void*), void (*unlock)(void*), void* lockContext)
{
    auto& p = *static_cast<Presenter*>(presenter);
    *layoutAfter = glLayout;
    if (!p.image || width <= 0 || height <= 0)
        return false;
    // The rows' memory, from an address the device's alignment allows.
    uintptr_t first = reinterpret_cast<uintptr_t>(pixels);
    uintptr_t end = first + (static_cast<uintptr_t>(height - 1) * stride + width) * 4;
    uintptr_t base = first / p.hostAlignment * p.hostAlignment;
    VkDeviceSize size = (end - base + p.hostAlignment - 1) / p.hostAlignment * p.hostAlignment;
    void* hostPointer = reinterpret_cast<void*>(base);
    VkMemoryHostPointerPropertiesEXT hostProperties { };
    hostProperties.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT;
    if (p.hostPointerProperties(p.device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, hostPointer, &hostProperties) != VK_SUCCESS || !hostProperties.memoryTypeBits)
        return false;
    VkExternalMemoryBufferCreateInfo external { };
    external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
    external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
    VkBufferCreateInfo bufferInfo { };
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = &external;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(p.device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        return false;
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(p.device, buffer, &requirements);
    VkImportMemoryHostPointerInfoEXT import { };
    import.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT;
    import.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
    import.pHostPointer = hostPointer;
    VkMemoryAllocateInfo allocateInfo { };
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext = &import;
    allocateInfo.allocationSize = size;
    allocateInfo.memoryTypeIndex = memoryType(p, hostProperties.memoryTypeBits & requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    VkDeviceMemory memory = VK_NULL_HANDLE;
    bool copied = false;
    if (allocateInfo.memoryTypeIndex != UINT32_MAX && vkAllocateMemory(p.device, &allocateInfo, nullptr, &memory) == VK_SUCCESS && vkBindBufferMemory(p.device, buffer, memory, 0) == VK_SUCCESS) {
        VkImageLayout layout = vulkanLayout(glLayout);
        VkCommandBufferBeginInfo begin { };
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkResetCommandBuffer(p.commands, 0);
        vkBeginCommandBuffer(p.commands, &begin);
        // (After all ANGLE submitted before: its frame. The copy reads the
        // image in TRANSFER_SRC, which it is left in for ANGLE again.)
        VkImageMemoryBarrier toSource { };
        toSource.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toSource.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        toSource.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toSource.oldLayout = layout;
        toSource.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toSource.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toSource.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toSource.image = p.image;
        toSource.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdPipelineBarrier(p.commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSource);
        VkBufferImageCopy region { };
        region.bufferOffset = first - base;
        region.bufferRowLength = static_cast<uint32_t>(stride);
        region.bufferImageHeight = static_cast<uint32_t>(height);
        region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
        vkCmdCopyImageToBuffer(p.commands, p.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
        VkBufferMemoryBarrier toHost { };
        toHost.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        toHost.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toHost.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        toHost.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toHost.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toHost.buffer = buffer;
        toHost.offset = 0;
        toHost.size = VK_WHOLE_SIZE;
        VkImageMemoryBarrier back = toSource;
        back.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        back.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        back.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        back.newLayout = layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : layout;
        if (layout == VK_IMAGE_LAYOUT_UNDEFINED)
            *layoutAfter = 0x9592;
        vkCmdPipelineBarrier(p.commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, &toHost, 1, &back);
        if (vkEndCommandBuffer(p.commands) == VK_SUCCESS) {
            VkSubmitInfo submit { };
            submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &p.commands;
            lock(lockContext);
            VkResult result = vkQueueSubmit(p.queue, 1, &submit, p.fence);
            unlock(lockContext);
            if (result == VK_SUCCESS) {
                copied = vkWaitForFences(p.device, 1, &p.fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;
                vkResetFences(p.device, 1, &p.fence);
            }
        }
    }
    vkDestroyBuffer(p.device, buffer, nullptr);
    if (memory)
        vkFreeMemory(p.device, memory, nullptr);
    return copied;
}
