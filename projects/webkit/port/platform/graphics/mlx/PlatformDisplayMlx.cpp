// The Mlx port's PlatformDisplay, GLContext and GLFence: what WebCore's
// Skia sources ask of the GPU when an image or a canvas is accelerated
// (PlatformDisplaySkia.cpp, GLContext.cpp and GLFence.cpp over EGL; not
// compiled, lists/webcore-mlx-skipped.txt). The port paints on the CPU
// (PORT.md, "Graphics"): there is no display, so nothing is accelerated,
// and WebCore asks for one only where an accelerated image or buffer
// exists.
//
// STUB (M8 and after: Skia's Ganesh over the compositor's GPU, std.gpu):
// no display exists; sharedDisplayIfExists() says so, and asking for the
// display itself is a crash.
#include "config.h"
#include "PlatformDisplay.h"

#include "GLContext.h"
#include "GLFence.h"

namespace WebCore {

PlatformDisplay* PlatformDisplay::sharedDisplayIfExists()
{
    return nullptr;
}

PlatformDisplay& PlatformDisplay::sharedDisplay()
{
    PlatformDisplay* display = sharedDisplayIfExists();
    RELEASE_ASSERT(display);
    return *display;
}

GLContext* PlatformDisplay::skiaGLContext()
{
    return nullptr;
}

GrDirectContext* PlatformDisplay::skiaGrContext() const
{
    return nullptr;
}

unsigned PlatformDisplay::msaaSampleCount() const
{
    return 0;
}

size_t PlatformDisplay::maxSkiaResourceCacheBytes() const
{
    return 0;
}

bool GLContext::makeContextCurrent()
{
    return false;
}

bool GLFence::isSupported(const GLDisplay&)
{
    return false;
}

std::unique_ptr<GLFence> GLFence::create(const GLDisplay&)
{
    return nullptr;
}

} // namespace WebCore
