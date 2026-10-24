// ANGLE's vma:: interface (vk_mem_alloc_wrapper.h) without the Vulkan
// Memory Allocator: WebKit's copy of ANGLE leaves third_party's
// vulkan_memory_allocator out, and the port builds no third-party library.
// What ANGLE asks of it, on Vulkan's own calls:
//
// - an allocation is a VkDeviceMemory of its own (lavapipe, the driver the
//   port runs on, allows billions of them), of the memory type VMA would
//   choose: of the resource's types and the required flags, the one with
//   the most of the preferred flags;
// - mapping is counted (a persistently mapped allocation stays mapped);
//   flushes and invalidations are aligned to nonCoherentAtomSize, and none
//   for coherent memory;
// - a virtual block (ANGLE's buffer suballocation) hands out ranges first
//   fit, the linear flag's order being an optimization only.
#include "libANGLE/renderer/vulkan/vk_mem_alloc_wrapper.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>

struct VmaAllocator_T
{
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkPhysicalDeviceMemoryProperties memory;
    VkDeviceSize atomSize;
    std::mutex lock;
    uint64_t allocationCount;
    uint64_t allocationBytes;
};

struct VmaAllocation_T
{
    VkDeviceMemory memory;
    VkDeviceSize size;
    uint32_t type;
    void *mapped;
    uint32_t mapCount;
    bool persistent;
};

struct VirtualRange
{
    VkDeviceSize size;
    void *userData;
};

struct VmaVirtualBlock_T
{
    VkDeviceSize size;
    std::map<VkDeviceSize, VirtualRange> ranges;
};

namespace vma
{
namespace
{
uint32_t BitCount(uint32_t value)
{
    uint32_t count = 0;
    for (; value; value &= value - 1)
        ++count;
    return count;
}

// VMA's choice: of the types allowed, those with every required flag, the
// one missing the fewest preferred flags.
VkResult ChooseType(VmaAllocator allocator,
                    uint32_t typeBits,
                    VkMemoryPropertyFlags required,
                    VkMemoryPropertyFlags preferred,
                    uint32_t *typeOut)
{
    uint32_t best = UINT32_MAX;
    uint32_t bestCost = UINT32_MAX;
    for (uint32_t type = 0; type < allocator->memory.memoryTypeCount; ++type)
    {
        if (!(typeBits & (1u << type)))
            continue;
        VkMemoryPropertyFlags flags = allocator->memory.memoryTypes[type].propertyFlags;
        if ((flags & required) != required)
            continue;
        uint32_t cost = BitCount(preferred & ~flags);
        if (cost < bestCost)
        {
            best = type;
            bestCost = cost;
        }
    }
    if (best == UINT32_MAX)
        return VK_ERROR_FEATURE_NOT_PRESENT;
    *typeOut = best;
    return VK_SUCCESS;
}

VkResult Allocate(VmaAllocator allocator,
                  const VkMemoryRequirements &requirements,
                  uint32_t typeBits,
                  VkMemoryPropertyFlags required,
                  VkMemoryPropertyFlags preferred,
                  bool mapped,
                  VmaAllocation *allocationOut)
{
    uint32_t type = 0;
    VkResult result = ChooseType(allocator, requirements.memoryTypeBits & typeBits, required, preferred, &type);
    if (result != VK_SUCCESS)
        return result;
    VkMemoryAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.allocationSize = requirements.size;
    info.memoryTypeIndex = type;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    result = vkAllocateMemory(allocator->device, &info, nullptr, &memory);
    if (result != VK_SUCCESS)
        return result;
    VmaAllocation allocation = new VmaAllocation_T();
    allocation->memory = memory;
    allocation->size = requirements.size;
    allocation->type = type;
    allocation->mapped = nullptr;
    allocation->mapCount = 0;
    allocation->persistent = false;
    bool hostVisible = allocator->memory.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    if (mapped && hostVisible)
    {
        result = vkMapMemory(allocator->device, memory, 0, VK_WHOLE_SIZE, 0, &allocation->mapped);
        if (result != VK_SUCCESS)
        {
            vkFreeMemory(allocator->device, memory, nullptr);
            delete allocation;
            return result;
        }
        allocation->persistent = true;
    }
    {
        std::lock_guard<std::mutex> guard(allocator->lock);
        allocator->allocationCount += 1;
        allocator->allocationBytes += requirements.size;
    }
    *allocationOut = allocation;
    return VK_SUCCESS;
}

// A range of an allocation for vkFlush/vkInvalidateMappedMemoryRanges, or
// false for coherent memory (nothing to do).
bool AtomRange(VmaAllocator allocator,
               VmaAllocation allocation,
               VkDeviceSize offset,
               VkDeviceSize size,
               VkMappedMemoryRange *range)
{
    if (allocator->memory.memoryTypes[allocation->type].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        return false;
    VkDeviceSize atom = allocator->atomSize ? allocator->atomSize : 1;
    VkDeviceSize start = offset / atom * atom;
    VkDeviceSize end = size == VK_WHOLE_SIZE ? allocation->size : offset + size;
    end = (end + atom - 1) / atom * atom;
    if (end > allocation->size)
        end = allocation->size;
    *range = {};
    range->sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range->memory = allocation->memory;
    range->offset = start;
    range->size = end == allocation->size ? VK_WHOLE_SIZE : end - start;
    return true;
}

VmaVirtualAllocation HandleOf(VkDeviceSize offset)
{
    return reinterpret_cast<VmaVirtualAllocation>(static_cast<uintptr_t>(offset + 1));
}
}  // namespace

VkResult InitAllocator(VkPhysicalDevice physicalDevice,
                       VkDevice device,
                       VkInstance instance,
                       uint32_t apiVersion,
                       VkDeviceSize preferredLargeHeapBlockSize,
                       VmaAllocator *pAllocator)
{
    VmaAllocator allocator = new VmaAllocator_T();
    allocator->physicalDevice = physicalDevice;
    allocator->device = device;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &allocator->memory);
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    allocator->atomSize = properties.limits.nonCoherentAtomSize;
    allocator->allocationCount = 0;
    allocator->allocationBytes = 0;
    *pAllocator = allocator;
    return VK_SUCCESS;
}

void DestroyAllocator(VmaAllocator allocator)
{
    delete allocator;
}

void FreeMemory(VmaAllocator allocator, VmaAllocation allocation)
{
    if (!allocation)
        return;
    if (allocation->mapped)
        vkUnmapMemory(allocator->device, allocation->memory);
    vkFreeMemory(allocator->device, allocation->memory, nullptr);
    {
        std::lock_guard<std::mutex> guard(allocator->lock);
        allocator->allocationCount -= 1;
        allocator->allocationBytes -= allocation->size;
    }
    delete allocation;
}

VkResult CreateBuffer(VmaAllocator allocator,
                      const VkBufferCreateInfo *pBufferCreateInfo,
                      VkMemoryPropertyFlags requiredFlags,
                      VkMemoryPropertyFlags preferredFlags,
                      bool persistentlyMapped,
                      uint32_t *pMemoryTypeIndexOut,
                      VkBuffer *pBuffer,
                      VmaAllocation *pAllocation)
{
    VkResult result = vkCreateBuffer(allocator->device, pBufferCreateInfo, nullptr, pBuffer);
    if (result != VK_SUCCESS)
        return result;
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(allocator->device, *pBuffer, &requirements);
    result = Allocate(allocator, requirements, UINT32_MAX, requiredFlags, preferredFlags, persistentlyMapped, pAllocation);
    if (result == VK_SUCCESS)
    {
        result = vkBindBufferMemory(allocator->device, *pBuffer, (*pAllocation)->memory, 0);
        if (result != VK_SUCCESS)
        {
            FreeMemory(allocator, *pAllocation);
            *pAllocation = VK_NULL_HANDLE;
        }
    }
    if (result != VK_SUCCESS)
    {
        vkDestroyBuffer(allocator->device, *pBuffer, nullptr);
        *pBuffer = VK_NULL_HANDLE;
        return result;
    }
    *pMemoryTypeIndexOut = (*pAllocation)->type;
    return VK_SUCCESS;
}

VkResult AllocateAndBindMemoryForImage(VmaAllocator allocator,
                                       VkImage *pImage,
                                       VkMemoryPropertyFlags requiredFlags,
                                       VkMemoryPropertyFlags preferredFlags,
                                       uint32_t memoryTypeBits,
                                       bool allocateDedicatedMemory,
                                       VmaAllocation *pAllocationOut,
                                       uint32_t *pMemoryTypeIndexOut,
                                       VkDeviceSize *sizeOut)
{
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(allocator->device, *pImage, &requirements);
    VkResult result = Allocate(allocator, requirements, memoryTypeBits ? memoryTypeBits : UINT32_MAX, requiredFlags, preferredFlags, false, pAllocationOut);
    if (result != VK_SUCCESS)
        return result;
    result = vkBindImageMemory(allocator->device, *pImage, (*pAllocationOut)->memory, 0);
    if (result != VK_SUCCESS)
    {
        FreeMemory(allocator, *pAllocationOut);
        *pAllocationOut = VK_NULL_HANDLE;
        return result;
    }
    *pMemoryTypeIndexOut = (*pAllocationOut)->type;
    *sizeOut = requirements.size;
    return VK_SUCCESS;
}

VkResult FindMemoryTypeIndexForBufferInfo(VmaAllocator allocator,
                                          const VkBufferCreateInfo *pBufferCreateInfo,
                                          VkMemoryPropertyFlags requiredFlags,
                                          VkMemoryPropertyFlags preferredFlags,
                                          bool persistentlyMappedBuffers,
                                          uint32_t *pMemoryTypeIndexOut)
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkResult result = vkCreateBuffer(allocator->device, pBufferCreateInfo, nullptr, &buffer);
    if (result != VK_SUCCESS)
        return result;
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(allocator->device, buffer, &requirements);
    vkDestroyBuffer(allocator->device, buffer, nullptr);
    return ChooseType(allocator, requirements.memoryTypeBits, requiredFlags, preferredFlags, pMemoryTypeIndexOut);
}

VkResult FindMemoryTypeIndexForImageInfo(VmaAllocator allocator,
                                         const VkImageCreateInfo *pImageCreateInfo,
                                         VkMemoryPropertyFlags requiredFlags,
                                         VkMemoryPropertyFlags preferredFlags,
                                         bool allocateDedicatedMemory,
                                         uint32_t *pMemoryTypeIndexOut)
{
    VkImage image = VK_NULL_HANDLE;
    VkResult result = vkCreateImage(allocator->device, pImageCreateInfo, nullptr, &image);
    if (result != VK_SUCCESS)
        return result;
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(allocator->device, image, &requirements);
    vkDestroyImage(allocator->device, image, nullptr);
    return ChooseType(allocator, requirements.memoryTypeBits, requiredFlags, preferredFlags, pMemoryTypeIndexOut);
}

void GetMemoryTypeProperties(VmaAllocator allocator, uint32_t memoryTypeIndex, VkMemoryPropertyFlags *pFlags)
{
    *pFlags = allocator->memory.memoryTypes[memoryTypeIndex].propertyFlags;
}

VkResult MapMemory(VmaAllocator allocator, VmaAllocation allocation, void **ppData)
{
    std::lock_guard<std::mutex> guard(allocator->lock);
    if (!allocation->mapped)
    {
        VkResult result = vkMapMemory(allocator->device, allocation->memory, 0, VK_WHOLE_SIZE, 0, &allocation->mapped);
        if (result != VK_SUCCESS)
        {
            allocation->mapped = nullptr;
            return result;
        }
    }
    allocation->mapCount += 1;
    *ppData = allocation->mapped;
    return VK_SUCCESS;
}

void UnmapMemory(VmaAllocator allocator, VmaAllocation allocation)
{
    std::lock_guard<std::mutex> guard(allocator->lock);
    if (allocation->mapCount == 0)
        return;
    allocation->mapCount -= 1;
    if (allocation->mapCount == 0 && !allocation->persistent)
    {
        vkUnmapMemory(allocator->device, allocation->memory);
        allocation->mapped = nullptr;
    }
}

void FlushAllocation(VmaAllocator allocator, VmaAllocation allocation, VkDeviceSize offset, VkDeviceSize size)
{
    VkMappedMemoryRange range;
    if (AtomRange(allocator, allocation, offset, size, &range))
        vkFlushMappedMemoryRanges(allocator->device, 1, &range);
}

void InvalidateAllocation(VmaAllocator allocator, VmaAllocation allocation, VkDeviceSize offset, VkDeviceSize size)
{
    VkMappedMemoryRange range;
    if (AtomRange(allocator, allocation, offset, size, &range))
        vkInvalidateMappedMemoryRanges(allocator->device, 1, &range);
}

void BuildStatsString(VmaAllocator allocator, char **statsString, VkBool32 detailedMap)
{
    char *text = static_cast<char *>(malloc(128));
    std::lock_guard<std::mutex> guard(allocator->lock);
    snprintf(text, 128, "{\"Total\": {\"BlockCount\": %llu, \"BlockBytes\": %llu}}",
             static_cast<unsigned long long>(allocator->allocationCount),
             static_cast<unsigned long long>(allocator->allocationBytes));
    *statsString = text;
}

void FreeStatsString(VmaAllocator allocator, char *statsString)
{
    free(statsString);
}

VkResult CreateVirtualBlock(VkDeviceSize size, VirtualBlockCreateFlags flags, VmaVirtualBlock *pVirtualBlock)
{
    VmaVirtualBlock block = new VmaVirtualBlock_T();
    block->size = size;
    *pVirtualBlock = block;
    return VK_SUCCESS;
}

void DestroyVirtualBlock(VmaVirtualBlock virtualBlock)
{
    delete virtualBlock;
}

VkResult VirtualAllocate(VmaVirtualBlock virtualBlock,
                         VkDeviceSize size,
                         VkDeviceSize alignment,
                         VmaVirtualAllocation *pAllocation,
                         VkDeviceSize *pOffset)
{
    if (alignment == 0)
        alignment = 1;
    VkDeviceSize start = 0;
    for (const auto &[offset, range] : virtualBlock->ranges)
    {
        VkDeviceSize candidate = (start + alignment - 1) / alignment * alignment;
        if (candidate + size <= offset)
        {
            start = candidate;
            break;
        }
        start = offset + range.size;
    }
    VkDeviceSize candidate = (start + alignment - 1) / alignment * alignment;
    auto at = virtualBlock->ranges.lower_bound(candidate);
    if (candidate + size > virtualBlock->size || (at != virtualBlock->ranges.end() && candidate + size > at->first))
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    virtualBlock->ranges[candidate] = { size, nullptr };
    *pAllocation = HandleOf(candidate);
    *pOffset = candidate;
    return VK_SUCCESS;
}

void VirtualFree(VmaVirtualBlock virtualBlock, VmaVirtualAllocation allocation, VkDeviceSize offset)
{
    virtualBlock->ranges.erase(offset);
}

VkBool32 IsVirtualBlockEmpty(VmaVirtualBlock virtualBlock)
{
    return virtualBlock->ranges.empty() ? VK_TRUE : VK_FALSE;
}

void GetVirtualAllocationInfo(VmaVirtualBlock virtualBlock,
                              VmaVirtualAllocation allocation,
                              VkDeviceSize offset,
                              VkDeviceSize *sizeOut,
                              void **pUserDataOut)
{
    auto at = virtualBlock->ranges.find(offset);
    *sizeOut = at == virtualBlock->ranges.end() ? 0 : at->second.size;
    *pUserDataOut = at == virtualBlock->ranges.end() ? nullptr : at->second.userData;
}

void ClearVirtualBlock(VmaVirtualBlock virtualBlock)
{
    virtualBlock->ranges.clear();
}

void SetVirtualAllocationUserData(VmaVirtualBlock virtualBlock, VkDeviceSize offset, void *pUserData)
{
    auto at = virtualBlock->ranges.find(offset);
    if (at != virtualBlock->ranges.end())
        at->second.userData = pUserData;
}

void CalculateVirtualBlockStats(VmaVirtualBlock virtualBlock, StatInfo *pStatInfo)
{
    *pStatInfo = {};
    pStatInfo->basicInfo.blockCount = 1;
    pStatInfo->basicInfo.blockBytes = virtualBlock->size;
    pStatInfo->allocationSizeMin = VK_WHOLE_SIZE;
    pStatInfo->unusedRangeSizeMin = VK_WHOLE_SIZE;
    VkDeviceSize end = 0;
    auto unused = [&](VkDeviceSize gap) {
        if (!gap)
            return;
        pStatInfo->unusedRangeCount += 1;
        if (gap < pStatInfo->unusedRangeSizeMin)
            pStatInfo->unusedRangeSizeMin = gap;
        if (gap > pStatInfo->unusedRangeSizeMax)
            pStatInfo->unusedRangeSizeMax = gap;
    };
    for (const auto &[offset, range] : virtualBlock->ranges)
    {
        pStatInfo->basicInfo.allocationCount += 1;
        pStatInfo->basicInfo.allocationBytes += range.size;
        if (range.size < pStatInfo->allocationSizeMin)
            pStatInfo->allocationSizeMin = range.size;
        if (range.size > pStatInfo->allocationSizeMax)
            pStatInfo->allocationSizeMax = range.size;
        unused(offset - end);
        end = offset + range.size;
    }
    unused(virtualBlock->size - end);
}

void BuildVirtualBlockStatsString(VmaVirtualBlock virtualBlock, char **ppStatsString, VkBool32 detailedMap)
{
    StatInfo info;
    CalculateVirtualBlockStats(virtualBlock, &info);
    char *text = static_cast<char *>(malloc(160));
    snprintf(text, 160, "{\"Size\": %llu, \"Allocations\": %u, \"AllocationBytes\": %llu}",
             static_cast<unsigned long long>(virtualBlock->size), info.basicInfo.allocationCount,
             static_cast<unsigned long long>(info.basicInfo.allocationBytes));
    *ppStatsString = text;
}

void FreeVirtualBlockStatsString(VmaVirtualBlock virtualBlock, char *pStatsString)
{
    free(pStatsString);
}
}  // namespace vma
