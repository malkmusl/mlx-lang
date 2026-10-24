// The Mlx port's GPU display: ANGLE's EGL display on Vulkan (libmlxvulkan.so,
// lavapipe or the system's driver) without a window system, which WebGL's
// contexts (GraphicsContextGLMlx.cpp) and the GPU compositor's
// (MlxLayerTreeHost, TextureMapper over GLES) are made on.

#pragma once

namespace WebCore {

// ANGLE's EGLDisplay, initialized once for the process; EGL_NO_DISPLAY
// (null) when there is no Vulkan device.
void* mlxANGLEDisplay();

// PlatformDisplay::sharedDisplay() made over mlxANGLEDisplay() (once, on
// the main thread: WebKit's GLDisplay is made there): what
// TextureMapper, BitmapTexturePool and GLContext ask for. False when there
// is no display or no GL context can be made on it.
bool mlxEnsureSharedDisplay();

} // namespace WebCore
