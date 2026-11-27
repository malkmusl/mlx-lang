# The sources of the conformance suite's port

The port (README.md) builds Khronos' Vulkan conformance suite as one
tag of it has it, with what that tag's `external/fetch_sources.py`
fetches for its build, and nothing of it is kept in this repository:
`tools/fetch_cts.sh` fetches them into a cache outside it
(`$CTS_SOURCES`, by default `mlx-out/cts/sources`) and holds each
against the record below (its commit, the tree of that commit, the
suite's pin of it). The sources are never edited; what the port needs
around them is in `port/` and `build.mlx`.

## Which tag, and why

vulkan-cts-1.3.10.0, the newest vulkan-cts-1.3.* tag (KhronosGroup's
VK-GL-CTS lists 1.3.0.0 to 1.3.10.0; 1.4.* are the suite of Vulkan 1.4,
which our driver, a Vulkan 1.1 device, is far from):

- its Vulkan headers and generated tables (`external/vulkancts/framework
  /vulkan/generated/vulkan/*.inl`, in the tree) are of a Vulkan-Headers
  older than the one std.vulkan is made from (1.4.362), so what the
  suite asks of the API our std and driver know;
- it has `--deqp-compute-only`, which leaves out the graphics queue a
  test would otherwise want: our device has a compute queue only;
- its compute group is `dEQP-VK.compute.pipeline.basic.*` (from 1.3.8
  on the compute tests are made once per pipeline construction type).

## The record

The suite:

    vk-gl-cts.url: https://github.com/KhronosGroup/VK-GL-CTS.git
    vk-gl-cts.commit: 7aa3551652743f11e77cc28dd8c19c9f872ecf79
    vk-gl-cts.tree: e5851e139d0ed09fcdc9077d737654ede037653f
    vk-gl-cts.tag: vulkan-cts-1.3.10.0
    vk-gl-cts.tag-object: 6cd0334d922fb3bf8347341f34a292fbd2c2b585
    vk-gl-cts.what: VK-GL-CTS vulkan-cts-1.3.10.0 (tag object 6cd0334d922fb3bf8347341f34a292fbd2c2b585)
    vk-gl-cts.paths: framework/delibs/ framework/common/ framework/qphelper/ framework/platform/ framework/xexml/ framework/randomshaders/ framework/referencerenderer/ framework/opengl/ framework/CMakeLists.txt external/fetch_sources.py external/vulkancts/framework/ external/vulkancts/modules/vulkan/ external/vulkancts/vkscserver/ external/vulkancts/README.md CMakeLists.txt targets/ modules/internal/ executor/ LICENSE README.md

glslang, which the port builds with mlx-cc (the suite's GLSL to SPIR-V):

    glslang.url: https://github.com/KhronosGroup/glslang.git
    glslang.commit: c5b76b78c9dec95251e9c1840a671e19bf61abe3
    glslang.tree: c1b0ef6d9103ae199727b698f51b6fd1707ff5fd
    glslang.what: glslang 14.3.0 and after, as the suite pins it
    glslang.fetch-sources: KhronosGroup/glslang.git
    glslang.paths: glslang/ SPIRV/ OGLCompilersDLL/ CMakeLists.txt CHANGES.md LICENSE.txt README.md build_info.py build_info.h.tmpl known_good.json External/CMakeLists.txt

SPIRV-Tools, of which the port takes the API headers only (its library
is not built: port/spirv_tools.cpp):

    spirv-tools.url: https://github.com/KhronosGroup/SPIRV-Tools.git
    spirv-tools.commit: 44936c4a9d42f1c67e34babb5792adf5bce7f76b
    spirv-tools.tree: 50ae2260e4a1bd140535cf7eace670e8cebc341e
    spirv-tools.what: SPIRV-Tools, its include/ only, as the suite pins it
    spirv-tools.fetch-sources: KhronosGroup/SPIRV-Tools.git
    spirv-tools.paths: include/ LICENSE README.md CHANGES

SPIRV-Headers, pinned by the suite too, is not fetched: glslang carries
the SPIR-V headers it needs (SPIRV/spirv.hpp) and nothing the port
builds includes SPIRV-Headers' (2a9b6f951c7d6b04b6c21fe1bf3f475b68b84801,
tree a31c4f2ae5b3041d1894125c01e9bab04809ccc4). The suite's other
externals (Vulkan-Docs, Amber, jsoncpp, Vulkan-ValidationLayers, video
parser, ESExtractor, RenderDoc) are not needed: the generated tables are
in the suite's tree, and the groups that need the others are not built.
