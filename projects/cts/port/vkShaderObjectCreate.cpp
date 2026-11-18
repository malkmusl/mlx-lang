// vk::createShader of a VkShaderCreateInfoEXT, which the suite's
// vkShaderObjectUtil.hpp declares and vkShaderObjectUtil.cpp defines
// `inline` without using it there: no object of that file holds it (as
// with g++, an inline function is made only where it is used), and
// vkComputePipelineConstructionUtil.cpp calls it for the shader object
// pipeline construction types. Its definition, as vkShaderObjectUtil.cpp
// has it, made here once.
#include "vkShaderObjectUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"

namespace vk
{

Move<VkShaderEXT> createShader(const DeviceInterface &vk, VkDevice device, const vk::VkShaderCreateInfoEXT &shaderCreateInfo)
{
    VkShaderEXT object;
    VK_CHECK(vk.createShadersEXT(device, 1u, &shaderCreateInfo, nullptr, &object));
    return Move<VkShaderEXT>(check<VkShaderEXT>(object), Deleter<VkShaderEXT>(vk, device, nullptr));
}

} // namespace vk
