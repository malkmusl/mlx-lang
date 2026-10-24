// The Mlx port's PlatformKeyboardEvent functions that a port supplies
// (PORT.md, "Everything else a port supplies"): what libWPE's
// PlatformKeyboardEventLibWPE.cpp and GTK's give the others. The embedder
// makes key events from Wayland's (std.xkb's key symbols) and tells
// WebCore the modifiers' state with setCurrentModifierState().
#include "config.h"
#include "PlatformKeyboardEvent.h"

namespace WebCore {

// The modifiers the embedder last set (none before it does), as GTK's
// and WPE's ports answer.
OptionSet<PlatformEvent::Modifier> PlatformKeyboardEvent::currentStateOfModifierKeys()
{
    if (s_currentModifiers)
        return *s_currentModifiers;
    return { };
}

// A key down event made into a raw key down (no text) or a char event
// (no key identifier), as libWPE's (no input method yet: M5).
void PlatformKeyboardEvent::disambiguateKeyDownEvent(Type type, bool backwardsCompatibility)
{
    ASSERT(m_type == PlatformEvent::Type::KeyDown);
    m_type = type;
    if (backwardsCompatibility)
        return;

    if (type == PlatformEvent::Type::RawKeyDown) {
        m_text = String();
        m_unmodifiedText = String();
    } else
        m_keyIdentifier = String();
}

} // namespace WebCore
