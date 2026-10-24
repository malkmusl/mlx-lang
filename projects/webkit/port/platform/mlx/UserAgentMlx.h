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

// The engine and its version as the user agent names it ("WebKit 2.54.1"),
// for an embedder's own pages (Voyager's voyager://about).
String mlxEngine();

} // namespace WebCore
