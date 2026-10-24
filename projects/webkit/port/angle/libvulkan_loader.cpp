// ANGLE's angle::vk::OpenLibVulkan (src/common/vulkan/libvulkan_loader.cpp
// opens the system's libvulkan.so, Khronos's loader) pointed at the port's
// Vulkan: libmlxvulkan.so (compiler/libc/vulkan), which the program
// already links. ANGLE's GPU information (gpu_info_util's
// SystemInfo_vulkan.cpp) takes vkGetInstanceProcAddr and the global
// commands from it, as the backend itself calls them.
#include "common/vulkan/libvulkan_loader.h"

#include "common/system_utils.h"

namespace angle
{
namespace vk
{
void *OpenLibVulkan()
{
    return OpenSystemLibraryWithExtension("libmlxvulkan.so.1", SearchType::AlreadyLoaded);
}
}  // namespace vk
}  // namespace angle
