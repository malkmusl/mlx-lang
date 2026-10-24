// mlx-vulkan's test: an instance, a device, a buffer filled by a command
// buffer submitted to a queue, through libmlxvulkan.so's dispatch (the
// objects' tables) and its vkGetInstanceProcAddr.
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { VkResult r_ = (x); if (r_ != VK_SUCCESS) { printf("%s failed: %d\n", #x, r_); return 1; } } while (0)
int main(void) {
    uint32_t version = 0;
    CHECK(vkEnumerateInstanceVersion(&version));
    printf("instance version %u.%u\n", VK_API_VERSION_MAJOR(version), VK_API_VERSION_MINOR(version));
    VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO, 0, "t", 1, "t", 1, VK_API_VERSION_1_3 };
    VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, 0, 0, &app, 0, 0, 0, 0 };
    VkInstance inst;
    CHECK(vkCreateInstance(&ici, 0, &inst));
    uint32_t n = 1; VkPhysicalDevice pd;
    VkResult r = vkEnumeratePhysicalDevices(inst, &n, &pd);
    printf("devices %d %u\n", r, n);
    VkPhysicalDeviceProperties props; vkGetPhysicalDeviceProperties(pd, &props);
    printf("device %s\n", props.deviceName);
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, 0, 0, 0, 1, &prio };
    VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, 0, 0, 1, &qci, 0, 0, 0, 0, 0 };
    VkDevice dev; CHECK(vkCreateDevice(pd, &dci, 0, &dev));
    VkQueue q; vkGetDeviceQueue(dev, 0, 0, &q);
    VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, 0, 0, 256, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, 0 };
    VkBuffer buf; CHECK(vkCreateBuffer(dev, &bci, 0, &buf));
    VkMemoryRequirements req; vkGetBufferMemoryRequirements(dev, buf, &req);
    VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(pd, &mp);
    uint32_t type = 0; while (!((req.memoryTypeBits >> type) & 1) || !(mp.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) type++;
    VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, 0, req.size, type };
    VkDeviceMemory mem; CHECK(vkAllocateMemory(dev, &mai, 0, &mem));
    CHECK(vkBindBufferMemory(dev, buf, mem, 0));
    VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, 0, 0, 0 };
    VkCommandPool pool; CHECK(vkCreateCommandPool(dev, &pci, 0, &pool));
    VkCommandBufferAllocateInfo cai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, 0, pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
    VkCommandBuffer cb; CHECK(vkAllocateCommandBuffers(dev, &cai, &cb));
    VkCommandBufferBeginInfo cbi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 0, 0, 0 };
    CHECK(vkBeginCommandBuffer(cb, &cbi));
    vkCmdFillBuffer(cb, buf, 0, 256, 0x12345678u);
    CHECK(vkEndCommandBuffer(cb));
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO, 0, 0, 0, 0, 1, &cb, 0, 0 };
    CHECK(vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE));
    CHECK(vkQueueWaitIdle(q));
    void *p; CHECK(vkMapMemory(dev, mem, 0, 256, 0, &p));
    printf("filled %x\n", ((unsigned *)p)[63]);
    PFN_vkGetPhysicalDeviceFeatures2KHR f2 = (PFN_vkGetPhysicalDeviceFeatures2KHR)vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceFeatures2KHR");
    printf("features2 %p\n", (void *)f2);
    vkUnmapMemory(dev, mem); vkFreeCommandBuffers(dev, pool, 1, &cb); vkDestroyCommandPool(dev, pool, 0); vkDestroyBuffer(dev, buf, 0); vkFreeMemory(dev, mem, 0);
    vkDestroyDevice(dev, 0); vkDestroyInstance(inst, 0);
    printf("done\n");
    return 0;
}
