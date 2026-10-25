// The kind of ANGLE's Vulkan device, for the port (PORT.md, "Compositing"):
// the compositor composes on the GPU on any device and names it, and
// whether it is a CPU's (VK_PHYSICAL_DEVICE_TYPE_CPU: lavapipe, llvmpipe,
// SwiftShader), in its trace. GraphicsContextGLMlx.cpp asks ANGLE's EGL device for
// its VkPhysicalDevice (EGL_ANGLE_device_vulkan) and asks here, where
// vulkan/vulkan.h is (mlx-vulkan's: libANGLE.a's sources have it).
#include <string.h>
#include <vulkan/vulkan.h>

// Whether PHYSICAL_DEVICE (a VkPhysicalDevice) is a CPU's; its name into
// NAME (SIZE bytes, terminated) when NAME is given.
extern "C" bool mlxVulkanDeviceIsCPU(void *physicalDevice, char *name, size_t size)
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(static_cast<VkPhysicalDevice>(physicalDevice), &properties);
    if (name && size)
    {
        strncpy(name, properties.deviceName, size - 1);
        name[size - 1] = 0;
    }
    return properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
}
