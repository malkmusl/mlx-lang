// The Mlx port's Pasteboard (PORT.md, "Clipboard"): until the embedder's
// PasteboardStrategy carries the Wayland data device's offers (M5), one
// pasteboard in the process, its strings by type (text/plain, text/html,
// text/uri-list and custom types), so copying and pasting within Voyager
// work. Drag and drop is off (ENABLE_DRAG_SUPPORT).
#include "config.h"
#include "Pasteboard.h"

#include "CommonAtomStrings.h"
#include "PasteboardContext.h"
#include "PasteboardCustomData.h"
#include "SharedBuffer.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

static MemoryCompactRobinHoodHashMap<String, String>& clipboard()
{
    static NeverDestroyed<MemoryCompactRobinHoodHashMap<String, String>> strings;
    return strings;
}

static constexpr ASCIILiteral uriListType = "text/uri-list"_s;

Pasteboard::Pasteboard(std::unique_ptr<PasteboardContext>&& context)
    : m_context(WTF::move(context))
{
}

std::unique_ptr<Pasteboard> Pasteboard::createForCopyAndPaste(std::unique_ptr<PasteboardContext>&& context)
{
    return makeUnique<Pasteboard>(WTF::move(context));
}

bool Pasteboard::hasData()
{
    return !clipboard().isEmpty();
}

Vector<String> Pasteboard::typesSafeForBindings(const String&)
{
    Vector<String> types;
    for (auto& type : clipboard().keys()) {
        if (isSafeTypeForDOMToReadAndWrite(type))
            types.append(type);
    }
    return types;
}

Vector<String> Pasteboard::typesForLegacyUnsafeBindings()
{
    return copyToVector(clipboard().keys());
}

String Pasteboard::readOrigin()
{
    return { };
}

String Pasteboard::readString(const String& type)
{
    return clipboard().get(type);
}

String Pasteboard::readStringInCustomData(const String& type)
{
    return clipboard().get(type);
}

void Pasteboard::writeString(const String& type, const String& data)
{
    clipboard().set(type, data);
}

void Pasteboard::clear()
{
    clipboard().clear();
}

void Pasteboard::clear(const String& type)
{
    clipboard().remove(type);
}

void Pasteboard::read(PasteboardPlainText& text, PlainTextURLReadingPolicy, std::optional<size_t>)
{
    text.text = clipboard().get(textPlainContentTypeAtom());
    if (text.text.isEmpty())
        text.text = clipboard().get(String { uriListType });
}

// Web content (markup, images, files) comes from the system's pasteboard
// through the embedder (M5); the process's own holds strings, which the
// editor reads as plain text.
void Pasteboard::read(PasteboardWebContentReader&, WebContentReadingPolicy, std::optional<size_t>)
{
}

void Pasteboard::read(PasteboardFileReader&, std::optional<size_t>)
{
}

void Pasteboard::write(const Color&)
{
}

void Pasteboard::write(const PasteboardURL& url)
{
    clear();
    clipboard().set(String { uriListType }, url.url.string());
    clipboard().set(textPlainContentTypeAtom(), url.url.string());
}

void Pasteboard::writeTrustworthyWebURLsPboardType(const PasteboardURL&)
{
}

void Pasteboard::write(const PasteboardImage& image)
{
    clear();
    if (!image.url.url.isEmpty())
        clipboard().set(String { uriListType }, image.url.url.string());
}

void Pasteboard::write(const PasteboardBuffer&)
{
}

void Pasteboard::write(const PasteboardWebContent&)
{
}

void Pasteboard::writeCustomData(const Vector<PasteboardCustomData>& data)
{
    for (auto& item : data) {
        item.forEachPlatformString([](auto& type, auto& string) {
            clipboard().set(type, string);
        });
        item.forEachCustomString([](auto& type, auto& string) {
            clipboard().set(type, string);
        });
    }
}

Pasteboard::FileContentState Pasteboard::fileContentState()
{
    return FileContentState::NoFileOrImageData;
}

bool Pasteboard::canSmartReplace()
{
    return false;
}

void Pasteboard::writeMarkup(const String& markup)
{
    clear();
    clipboard().set(textHTMLContentTypeAtom(), markup);
}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption)
{
    clear();
    clipboard().set(textPlainContentTypeAtom(), text);
}

} // namespace WebCore
