// The Mlx port's GPU display: ANGLE's EGL display on Vulkan (libmlxvulkan.so,
// lavapipe or the system's driver) without a window system, which WebGL's
// contexts (GraphicsContextGLMlx.cpp) and the GPU compositor's
// (MlxLayerTreeHost, TextureMapper over GLES) are made on.

#pragma once

namespace WebCore {

// ANGLE's EGLDisplay, initialized once for the process; EGL_NO_DISPLAY
// (null) when there is no Vulkan device.
void* mlxANGLEDisplay();

// Whether ANGLE's Vulkan device is a CPU's (its VkPhysicalDeviceProperties'
// deviceType VK_PHYSICAL_DEVICE_TYPE_CPU: lavapipe, llvmpipe,
// SwiftShader), and its name (NAME, when given: "" without a device).
// False without a display.
bool mlxANGLEDeviceIsCPU(const char** name = nullptr);

// PlatformDisplay::sharedDisplay() made over mlxANGLEDisplay() (once, on
// the main thread: WebKit's GLDisplay is made there): what
// TextureMapper, BitmapTexturePool and GLContext ask for. False when there
// is no display or no GL context can be made on it.
bool mlxEnsureSharedDisplay();

// Whether Ganesh paints on the display (MLX_PAINTING=gpu): a layer's tiles
// with the GPU's compositor, and the canvases WebKit asks to accelerate
// (RenderingPurpose::Canvas: a WebGL canvas drawn into a 2D one).
// Otherwise PlatformDisplay::skiaGLContext() is null and they are
// painted on the CPU.
bool mlxGPUPaintingWanted();

} // namespace WebCore
