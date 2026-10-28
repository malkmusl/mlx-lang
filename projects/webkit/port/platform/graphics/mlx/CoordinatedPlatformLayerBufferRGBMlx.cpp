// WebKit's CoordinatedPlatformLayerBufferRGB (a layer's contents in a GL
// texture: an image uploaded by the GPU compositor, a WebGL canvas'
// texture), compiled as it is. Its Ganesh half names GL_RGBA8, which
// libepoxy's GL header has and GLES 2's (ANGLE's copy, the port's:
// TextureMapperGLHeaders.h without USE_LIBEPOXY) has only as
// GL_RGBA8_OES, the same value; it is given here.

#include "config.h"
#include "TextureMapperGLHeaders.h"

#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif

#include "platform/graphics/texmap/coordinated/CoordinatedPlatformLayerBufferRGB.cpp"
