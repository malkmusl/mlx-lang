// The Mlx port's MIME types of file extensions (PORT.md: a table, in place
// of xdgmime; std.xdg's shared-mime-info database later): what file:
// loads and downloads are typed by.
#include "config.h"
#include "MIMETypeRegistry.h"

#include <wtf/text/MakeString.h>

namespace WebCore {

struct ExtensionMap {
    ASCIILiteral extension;
    ASCIILiteral mimeType;
};

// The first extension of a type is its preferred one.
static constexpr ExtensionMap extensionMap[] = {
    { "html"_s, "text/html"_s },
    { "htm"_s, "text/html"_s },
    { "shtml"_s, "text/html"_s },
    { "xhtml"_s, "application/xhtml+xml"_s },
    { "xht"_s, "application/xhtml+xml"_s },
    { "xml"_s, "text/xml"_s },
    { "xsl"_s, "text/xsl"_s },
    { "xslt"_s, "text/xsl"_s },
    { "svg"_s, "image/svg+xml"_s },
    { "svgz"_s, "image/svg+xml"_s },
    { "mml"_s, "application/mathml+xml"_s },
    { "txt"_s, "text/plain"_s },
    { "text"_s, "text/plain"_s },
    { "css"_s, "text/css"_s },
    { "js"_s, "text/javascript"_s },
    { "mjs"_s, "text/javascript"_s },
    { "json"_s, "application/json"_s },
    { "wasm"_s, "application/wasm"_s },
    { "csv"_s, "text/csv"_s },
    { "vtt"_s, "text/vtt"_s },
    { "md"_s, "text/markdown"_s },
    { "png"_s, "image/png"_s },
    { "apng"_s, "image/apng"_s },
    { "jpg"_s, "image/jpeg"_s },
    { "jpeg"_s, "image/jpeg"_s },
    { "jpe"_s, "image/jpeg"_s },
    { "jfif"_s, "image/jpeg"_s },
    { "gif"_s, "image/gif"_s },
    { "webp"_s, "image/webp"_s },
    { "bmp"_s, "image/bmp"_s },
    { "ico"_s, "image/vnd.microsoft.icon"_s },
    { "cur"_s, "image/x-icon"_s },
    { "avif"_s, "image/avif"_s },
    { "jxl"_s, "image/jxl"_s },
    { "tif"_s, "image/tiff"_s },
    { "tiff"_s, "image/tiff"_s },
    { "woff"_s, "font/woff"_s },
    { "woff2"_s, "font/woff2"_s },
    { "ttf"_s, "font/ttf"_s },
    { "otf"_s, "font/otf"_s },
    { "ttc"_s, "font/collection"_s },
    { "pdf"_s, "application/pdf"_s },
    { "zip"_s, "application/zip"_s },
    { "gz"_s, "application/gzip"_s },
    { "tar"_s, "application/x-tar"_s },
    { "mht"_s, "multipart/related"_s },
    { "mhtml"_s, "multipart/related"_s },
    { "mp4"_s, "video/mp4"_s },
    { "m4v"_s, "video/mp4"_s },
    { "webm"_s, "video/webm"_s },
    { "mkv"_s, "video/x-matroska"_s },
    { "ogv"_s, "video/ogg"_s },
    { "mp3"_s, "audio/mpeg"_s },
    { "m4a"_s, "audio/mp4"_s },
    { "ogg"_s, "audio/ogg"_s },
    { "oga"_s, "audio/ogg"_s },
    { "opus"_s, "audio/ogg"_s },
    { "flac"_s, "audio/flac"_s },
    { "wav"_s, "audio/wav"_s },
};

String MIMETypeRegistry::mimeTypeForExtension(StringView extension)
{
    for (auto& entry : extensionMap) {
        if (equalIgnoringASCIICase(extension, entry.extension))
            return entry.mimeType;
    }
    return String();
}

Vector<String> MIMETypeRegistry::extensionsForMIMEType(const String& mimeType)
{
    Vector<String> extensions;
    for (auto& entry : extensionMap) {
        if (equalIgnoringASCIICase(mimeType, entry.mimeType))
            extensions.append(entry.extension);
    }
    return extensions;
}

String MIMETypeRegistry::preferredExtensionForMIMEType(const String& mimeType)
{
    for (auto& entry : extensionMap) {
        if (equalIgnoringASCIICase(mimeType, entry.mimeType))
            return entry.extension;
    }
    return String();
}

// No plug-ins in this port.
bool MIMETypeRegistry::isApplicationPluginMIMEType(const String&)
{
    return false;
}

} // namespace WebCore
