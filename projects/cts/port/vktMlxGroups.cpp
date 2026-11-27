// The groups of the conformance suite this port builds, and those it does
// not build yet (projects/cts/README.md). vktTestPackage.cpp, compiled as
// it is, names every group of the suite; the files of the groups below
// are compiled, and each group is put together here (in place of
// api/vktApiTests.cpp, memory/vktMemoryTests.cpp,
// compute/vktComputeTests.cpp and
// synchronization/vktSynchronizationTests.cpp, which name subgroups that
// are not built yet):
//
//   dEQP-VK.info.*                   vktInfoTests.cpp
//   dEQP-VK.api.smoke.*              api/vktApiSmokeTests.cpp
//   dEQP-VK.api.info.*               api/vktApiFeatureInfo.cpp
//   dEQP-VK.api.device_init.*        api/vktApiDeviceInitializationTests.cpp
//   dEQP-VK.api.object_management.*  api/vktApiObjectManagementTests.cpp
//   dEQP-VK.api.buffer.*             api/vktApiBufferTests.cpp
//   dEQP-VK.memory.allocation.*      memory/vktMemoryAllocationTests.cpp
//   dEQP-VK.compute.pipeline.basic.* compute/vktComputeBasicComputeShaderTests.cpp
//   dEQP-VK.synchronization.basic.*  synchronization/vktSynchronizationBasic*.cpp
//
// Every other group is here by its name with no case in it: the suite
// lists it and runs nothing of it, and its files are not compiled.
#include "tcuTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktShaderLibrary.hpp"
#include "vktApiSmokeTests.hpp"
#include "vktApiFeatureInfo.hpp"
#include "vktApiDeviceInitializationTests.hpp"
#include "vktApiObjectManagementTests.hpp"
#include "vktApiBufferTests.hpp"
#include "vktMemoryAllocationTests.hpp"
#include "vktComputeBasicComputeShaderTests.hpp"
#include "vktSynchronizationBasicEventTests.hpp"
#include "vktSynchronizationBasicFenceTests.hpp"
#include "vktSynchronizationBasicSemaphoreTests.hpp"
#include "vktSynchronizationTests.hpp"
#include "vktRenderPassTests.hpp"
#include "vktAmberGraphicsFuzzTests.hpp"
#include "vktAmberGlslTests.hpp"
#include "vktAmberDepthTests.hpp"
#include "vktBindingModelTests.hpp"
#include "vktDGCTests.hpp"
#include "vktDescriptorIndexingTests.hpp"
#include "vktDeviceGroupTests.hpp"
#include "vktDrawTests.hpp"
#include "vktDynamicStateTests.hpp"
#include "vktFragmentOperationsTests.hpp"
#include "vktFragmentShaderInterlockTests.hpp"
#include "vktFragmentShadingBarycentricTests.hpp"
#include "vktFragmentShadingRateTests.hpp"
#include "vktMemoryModelTests.hpp"
#include "vktMeshShaderTests.hpp"
#include "vktMultiViewTests.hpp"
#include "vktProtectedMemTests.hpp"
#include "vktQueryPoolTests.hpp"
#include "vktRayQueryTests.hpp"
#include "vktRayTracingTests.hpp"
#include "vktReconvergenceTests.hpp"
#include "vktShaderObjectTests.hpp"
#include "vktSpvAsmTests.hpp"
#include "vktTransformFeedbackTests.hpp"
#include "vktClippingTests.hpp"
#include "vktConditionalTests.hpp"
#include "vktGeometryTests.hpp"
#include "vktImageTests.hpp"
#include "vktImagelessFramebufferTests.hpp"
#include "vktModifiersTests.hpp"
#include "vktPipelineTests.hpp"
#include "vktPostmortemTests.hpp"
#include "vktRasterizationTests.hpp"
#include "vktRobustnessTests.hpp"
#include "vktSparseResourcesTests.hpp"
#include "vktSSBOLayoutTests.hpp"
#include "vktSubgroupsTests.hpp"
#include "vktTessellationTests.hpp"
#include "vktTextureTests.hpp"
#include "vktUniformBlockTests.hpp"
#include "vktVideoTests.hpp"
#include "vktWsiTests.hpp"
#include "vktYCbCrTests.hpp"
#include "vktShaderRenderDerivateTests.hpp"
#include "vktShaderRenderDiscardTests.hpp"
#include "vktShaderRenderIndexingTests.hpp"
#include "vktShaderRenderInvarianceTests.hpp"
#include "vktShaderRenderLimitTests.hpp"
#include "vktShaderRenderLoopTests.hpp"
#include "vktShaderRenderMatrixTests.hpp"
#include "vktShaderRenderOperatorTests.hpp"
#include "vktShaderRenderReturnTests.hpp"
#include "vktShaderRenderStructTests.hpp"
#include "vktShaderRenderSwitchTests.hpp"
#include "vktShaderRenderTextureFunctionTests.hpp"
#include "vktShaderRenderTextureGatherTests.hpp"
#include "vktShaderRenderBuiltinVarTests.hpp"
#include "vktShaderBuiltinTests.hpp"
#include "vktOpaqueTypeIndexingTests.hpp"
#include "vktAtomicOperationTests.hpp"
#include "vktShaderClockTests.hpp"
#include "vktShaderExpectAssumeTests.hpp"

namespace vkt
{
namespace
{

// A group of the suite not built in this port: its name, no case.
tcu::TestCaseGroup *notBuilt(tcu::TestContext &testCtx, const std::string &name)
{
    return new tcu::TestCaseGroup(testCtx, name.c_str(), "not built in this port yet (projects/cts/README.md)");
}

void apiChildren(tcu::TestCaseGroup *group)
{
    tcu::TestContext &testCtx = group->getTestContext();
    group->addChild(api::createSmokeTests(testCtx));
    group->addChild(api::createFeatureInfoTests(testCtx));
    group->addChild(api::createDeviceInitializationTests(testCtx));
    group->addChild(api::createObjectManagementTests(testCtx));
    group->addChild(api::createBufferTests(testCtx));
}

void memoryChildren(tcu::TestCaseGroup *group)
{
    group->addChild(memory::createAllocationTests(group->getTestContext()));
}

void computePipelineChildren(tcu::TestCaseGroup *group)
{
    group->addChild(compute::createBasicComputeShaderTests(group->getTestContext(), vk::COMPUTE_PIPELINE_CONSTRUCTION_TYPE_PIPELINE));
}

void computeChildren(tcu::TestCaseGroup *group)
{
    group->addChild(createTestGroup(group->getTestContext(), "pipeline", computePipelineChildren));
}

void synchronizationBasicChildren(tcu::TestCaseGroup *group)
{
    tcu::TestContext &testCtx = group->getTestContext();
    group->addChild(synchronization::createBasicEventTests(testCtx));
    group->addChild(synchronization::createBasicFenceTests(testCtx));
    group->addChild(synchronization::createBasicBinarySemaphoreTests(testCtx, synchronization::SynchronizationType::LEGACY));
    group->addChild(synchronization::createBasicTimelineSemaphoreTests(testCtx, synchronization::SynchronizationType::LEGACY));
}

void synchronizationChildren(tcu::TestCaseGroup *group)
{
    group->addChild(createTestGroup(group->getTestContext(), "basic", synchronizationBasicChildren));
}

} // namespace

namespace api
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return createTestGroup(testCtx, name, apiChildren);
}
} // namespace api

namespace memory
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return createTestGroup(testCtx, name, memoryChildren);
}
} // namespace memory

namespace compute
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return createTestGroup(testCtx, name, computeChildren);
}
} // namespace compute

tcu::TestCaseGroup *createSynchronizationTests(tcu::TestContext &testCtx, const std::string &name)
{
    return createTestGroup(testCtx, name, synchronizationChildren);
}

// The groups not built.

tcu::TestCaseGroup *createSynchronization2Tests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}

tcu::TestCaseGroup *createRenderPassTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}

tcu::TestCaseGroup *createRenderPass2Tests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}

tcu::TestCaseGroup *createDynamicRenderingTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}

// (The GLSL group's shader library: its .test files are not read.)
de::MovePtr<tcu::TestCaseGroup> createShaderLibraryGroup(tcu::TestContext &testCtx, const std::string &name, const std::string &)
{
    return de::MovePtr<tcu::TestCaseGroup>(notBuilt(testCtx, name));
}

namespace cts_amber
{
tcu::TestCaseGroup *createGraphicsFuzzTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
tcu::TestCaseGroup *createAmberDepthGroup(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
tcu::TestCaseGroup *createCombinedOperationsGroup(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "combined_operations");
}
tcu::TestCaseGroup *createCrashTestGroup(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "crash_test");
}
} // namespace cts_amber

namespace BindingModel
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace BindingModel

namespace DGC
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace DGC

namespace DescriptorIndexing
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace DescriptorIndexing

namespace DeviceGroup
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace DeviceGroup

namespace Draw
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace Draw

namespace DynamicState
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace DynamicState

namespace FragmentOperations
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace FragmentOperations

namespace FragmentShaderInterlock
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace FragmentShaderInterlock

namespace FragmentShadingBarycentric
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace FragmentShadingBarycentric

namespace FragmentShadingRate
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace FragmentShadingRate

namespace MemoryModel
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace MemoryModel

namespace MeshShader
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace MeshShader

namespace MultiView
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace MultiView

namespace ProtectedMem
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace ProtectedMem

namespace QueryPool
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace QueryPool

namespace RayQuery
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace RayQuery

namespace RayTracing
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace RayTracing

namespace Reconvergence
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
tcu::TestCaseGroup *createTestsExperimental(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace Reconvergence

namespace ShaderObject
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace ShaderObject

namespace SpirVAssembly
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace SpirVAssembly

namespace TransformFeedback
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace TransformFeedback

namespace clipping
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace clipping

namespace conditional
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace conditional

namespace geometry
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace geometry

namespace image
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace image

namespace imageless
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace imageless

namespace modifiers
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace modifiers

namespace pipeline
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace pipeline

namespace postmortem
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace postmortem

namespace rasterization
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace rasterization

namespace robustness
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace robustness

namespace sparse
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace sparse

namespace ssbo
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace ssbo

namespace subgroups
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace subgroups

namespace tessellation
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace tessellation

namespace texture
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace texture

namespace ubo
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace ubo

namespace video
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace video

namespace wsi
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace wsi

namespace ycbcr
{
tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return notBuilt(testCtx, name);
}
} // namespace ycbcr

namespace sr
{
tcu::TestCaseGroup *createDerivateTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "derivate");
}
tcu::TestCaseGroup *createDiscardTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "discard");
}
tcu::TestCaseGroup *createDemoteTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "demote");
}
tcu::TestCaseGroup *createIndexingTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "indexing");
}
tcu::TestCaseGroup *createShaderInvarianceTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "invariance");
}
tcu::TestCaseGroup *createLimitTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "limits");
}
tcu::TestCaseGroup *createLoopTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "loops");
}
tcu::TestCaseGroup *createMatrixTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "matrix");
}
tcu::TestCaseGroup *createOperatorTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "operator");
}
tcu::TestCaseGroup *createReturnTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "return");
}
tcu::TestCaseGroup *createStructTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "struct");
}
tcu::TestCaseGroup *createSwitchTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "switch");
}
tcu::TestCaseGroup *createTextureFunctionTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "texture_functions");
}
tcu::TestCaseGroup *createTextureGatherTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "texture_gather");
}
tcu::TestCaseGroup *createBuiltinVarTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "builtin_var");
}
} // namespace sr

namespace shaderexecutor
{
tcu::TestCaseGroup *createBuiltinTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "builtin");
}
tcu::TestCaseGroup *createOpaqueTypeIndexingTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "opaque_type_indexing");
}
tcu::TestCaseGroup *createAtomicOperationTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "atomic_operations");
}
tcu::TestCaseGroup *createShaderClockTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "shader_clock");
}
tcu::TestCaseGroup *createShaderExpectAssumeTests(tcu::TestContext &testCtx)
{
    return notBuilt(testCtx, "shader_expect_assume");
}
} // namespace shaderexecutor

} // namespace vkt
