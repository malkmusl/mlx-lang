// The Mlx port's user agent with the embedder's part (UserAgentMlx.cpp,
// PORT.md "The user agent").
#pragma once

#include <wtf/Forward.h>

namespace WebCore {

// WebKit's compatibility string with "NAME/VERSION (ENGINE; WINDOW_SYSTEM)"
// after the platform's parentheses:
// the embedder's name and version, the engine and its version, and the
// window system its windows are shown by ("Wayland"; none for a program
// without windows).
String mlxUserAgent(const String& name, const String& version, const String& windowSystem);

} // namespace WebCore
