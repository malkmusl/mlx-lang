// The suite's Amber test case (modules/vulkan/amber/vktAmberTestCase.hpp)
// for a port that does not build Amber (README.md): groups that are
// built make some of their cases from Amber scripts (the compute group's
// `compute.pipeline.basic.*` among them), and those cases are here, each
// answering "not supported" when it is run. What a case asks for is kept
// as the real one keeps it; no script is read.
#include "vktAmberTestCase.hpp"
#include "tcuDefs.hpp"

namespace vkt
{
namespace cts_amber
{

tcu::TestStatus AmberTestInstance::iterate(void)
{
    TCU_THROW(NotSupportedError, "Amber is not built in this port: the case's script is not run");
}

AmberTestCase::AmberTestCase(tcu::TestContext &testCtx, const char *name, const char *description,
                             const std::string &readFilename)
    : TestCase(testCtx, name)
    , m_recipe(nullptr)
    , m_readFilename(readFilename)
{
    DE_UNREF(description);
}

AmberTestCase::~AmberTestCase(void)
{
}

TestInstance *AmberTestCase::createInstance(Context &ctx) const
{
    return new AmberTestInstance(ctx, m_recipe, VK_NULL_HANDLE);
}

void AmberTestCase::checkSupport(Context &) const
{
    TCU_THROW(NotSupportedError, "Amber is not built in this port: the case's script is not run");
}

void AmberTestCase::setSpirVAsmBuildOptions(const vk::SpirVAsmBuildOptions &asm_options)
{
    m_asm_options = asm_options;
}

void AmberTestCase::delayedInit(void)
{
}

void AmberTestCase::initPrograms(vk::SourceCollections &) const
{
}

void AmberTestCase::addRequirement(const std::string &requirement)
{
    if (requirement.find('.') != std::string::npos)
        m_required_features.insert(requirement);
    else
        m_required_extensions.insert(requirement);
}

void AmberTestCase::addPropertyRequirement(const std::string &requirement)
{
    m_required_properties.insert(requirement);
}

void AmberTestCase::addImageRequirement(vk::VkImageCreateInfo info)
{
    m_imageRequirements.push_back(info);
}

void AmberTestCase::addBufferRequirement(BufferRequirement req)
{
    m_bufferRequirements.push_back(req);
}

bool AmberTestCase::validateRequirements()
{
    return true;
}

bool AmberTestCase::parse(const std::string &)
{
    return false;
}

static AmberTestCase *made(tcu::TestContext &testCtx, const char *name, const char *description,
                           const char *category, const std::string &filename,
                           const std::vector<std::string> &requirements,
                           const std::vector<vk::VkImageCreateInfo> &imageRequirements,
                           const std::vector<BufferRequirement> &bufferRequirements)
{
    // (Where the real one reads the script: vulkan/amber/CATEGORY/FILE.)
    const std::string path = std::string("vulkan/amber/") + category + "/" + filename;
    AmberTestCase *testCase = new AmberTestCase(testCtx, name, description, path);
    for (const std::string &requirement : requirements)
        testCase->addRequirement(requirement);
    for (const vk::VkImageCreateInfo &image : imageRequirements)
        testCase->addImageRequirement(image);
    for (const BufferRequirement &buffer : bufferRequirements)
        testCase->addBufferRequirement(buffer);
    return testCase;
}

AmberTestCase *createAmberTestCase(tcu::TestContext &testCtx, const char *name, const char *category,
                                   const std::string &filename, const std::vector<std::string> requirements,
                                   const std::vector<vk::VkImageCreateInfo> imageRequirements,
                                   const std::vector<BufferRequirement> bufferRequirements)
{
    return made(testCtx, name, "", category, filename, requirements, imageRequirements, bufferRequirements);
}

AmberTestCase *createAmberTestCase(tcu::TestContext &testCtx, const char *name, const char *description,
                                   const char *category, const std::string &filename,
                                   const std::vector<std::string> requirements,
                                   const std::vector<vk::VkImageCreateInfo> imageRequirements,
                                   const std::vector<BufferRequirement> bufferRequirements)
{
    return made(testCtx, name, description, category, filename, requirements, imageRequirements,
                bufferRequirements);
}

void createAmberTestsFromIndexFile(tcu::TestContext &, tcu::TestCaseGroup *, const std::string, const char *)
{
}

} // namespace cts_amber
} // namespace vkt
