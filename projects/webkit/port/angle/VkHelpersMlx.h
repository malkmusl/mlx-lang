// Read before ANGLE's src/libANGLE/renderer/vulkan/vk_helpers.cpp (its
// -include): WebKit's changes to ANGLE (changes.diff) call
// InternalFormat::computeRowDepthSkipBytes there with the extents as one
// argument, an overload WebKit's formatutils.h does not have (WebKit
// builds no Vulkan backend; the port does). A call of seven arguments is
// the eight-argument function's, the extents' width and height apart; a
// call or declaration of eight is left as it is.
#ifndef VkHelpersMlx_h
#define VkHelpersMlx_h

#define MLX_ROW_DEPTH_SKIP_PICK(a1, a2, a3, a4, a5, a6, a7, a8, chosen, ...) chosen
#define MLX_ROW_DEPTH_SKIP_EXTENTS(type, extents, unpack, is3D, rowPitch, depthPitch, skipBytes) \
    computeRowDepthSkipBytes(type, (extents).width, (extents).height, unpack, is3D, rowPitch, depthPitch, skipBytes)
#define computeRowDepthSkipBytes(...) \
    MLX_ROW_DEPTH_SKIP_PICK(__VA_ARGS__, computeRowDepthSkipBytes(__VA_ARGS__), MLX_ROW_DEPTH_SKIP_EXTENTS(__VA_ARGS__), unused)

#endif
