/* The conformance suite's platform on our toolchain (projects/cts/README.md):
 * the framework's tcu::Platform with no window system, whose Vulkan
 * library is our driver over the host card (libmlxcts-driver.so,
 * MlxCtsDriver.h), or the Vulkan library --deqp-vk-library-path names
 * (one that exports vkGetInstanceProcAddr: libmlxvulkan.so with a
 * driver's manifest, to hold the suite itself against another driver).
 * tcuMain.cpp's main() asks createPlatform() for it. */
#include "tcuPlatform.hpp"
#include "tcuFunctionLibrary.hpp"
#include "vkPlatform.hpp"
#include "MlxCtsDriver.h"

#include <string.h>
#include <ostream>

namespace mlxcts
{

// Our driver's commands: vkGetInstanceProcAddr is the library's, every
// other global command what it gives without an instance.
class DriverFunctions : public tcu::FunctionLibrary
{
public:
    deFunctionPtr getFunction(const char *name) const
    {
        if (strcmp(name, "vkGetInstanceProcAddr") == 0)
            return (deFunctionPtr)&mlxcts_vk_get_instance_proc_addr;
        return (deFunctionPtr)mlxcts_vk_get_instance_proc_addr(nullptr, name);
    }
};

class DriverLibrary : public vk::Library
{
public:
    DriverLibrary(void) : m_driver(m_functions)
    {
    }

    const vk::PlatformInterface &getPlatformInterface(void) const
    {
        return m_driver;
    }

    const tcu::FunctionLibrary &getFunctionLibrary(void) const
    {
        return m_functions;
    }

private:
    const DriverFunctions m_functions;
    const vk::PlatformDriver m_driver;
};

// A Vulkan library loaded by its path (dlopen).
class LoadedLibrary : public vk::Library
{
public:
    LoadedLibrary(const char *path) : m_functions(path), m_driver(m_functions)
    {
    }

    const vk::PlatformInterface &getPlatformInterface(void) const
    {
        return m_driver;
    }

    const tcu::FunctionLibrary &getFunctionLibrary(void) const
    {
        return m_functions;
    }

private:
    const tcu::DynamicFunctionLibrary m_functions;
    const vk::PlatformDriver m_driver;
};

class VulkanPlatform : public vk::Platform
{
public:
    // (On a Unix target the suite asks for a library by its type too:
    // Vulkan's is ours, another one, the video parser's, is not built.)
#ifdef DE_PLATFORM_USE_LIBRARY_TYPE
    vk::Library *createLibrary(LibraryType libraryType, const char *path) const
    {
        if (libraryType != LIBRARY_TYPE_VULKAN)
            TCU_THROW(NotSupportedError, "only the Vulkan library is in this port");
        return createVulkanLibrary(path);
    }
#else
    vk::Library *createLibrary(const char *path) const
    {
        return createVulkanLibrary(path);
    }
#endif

    vk::Library *createVulkanLibrary(const char *path) const
    {
        if (path != nullptr && path[0] != 0)
            return new LoadedLibrary(path);
        return new DriverLibrary();
    }

    void describePlatform(std::ostream &dst) const
    {
        dst << "OS: brixOS port of the suite, run on the host\n";
        dst << "Driver: our own (projects/brix/vulkan) over the host card, the user's RX 580 with one compute ring\n";
    }
};

class Platform : public tcu::Platform
{
public:
    const vk::Platform &getVulkanPlatform(void) const
    {
        return m_vulkan;
    }

private:
    VulkanPlatform m_vulkan;
};

} // namespace mlxcts

tcu::Platform *createPlatform(void)
{
    return new mlxcts::Platform();
}
