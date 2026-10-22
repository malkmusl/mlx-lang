// The Mlx port's Skia font manager (SkFontMgrMlx.h).
#include "config.h"
#include "SkFontMgrMlx.h"

#include "port/MlxWebPort.h"
#include <skia/core/SkData.h>
#include <skia/core/SkFontArguments.h>
#include <skia/core/SkStream.h>
#include <src/ports/SkTypeface_FreeType.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

sk_sp<SkFontMgr> SkFontMgrMlx::create()
{
    return sk_make_sp<SkFontMgrMlx>();
}

int SkFontMgrMlx::fontconfigWidth(const SkFontStyle& style)
{
    switch (style.width()) {
    case SkFontStyle::kUltraCondensed_Width:
        return 50;
    case SkFontStyle::kExtraCondensed_Width:
        return 63;
    case SkFontStyle::kCondensed_Width:
        return 75;
    case SkFontStyle::kSemiCondensed_Width:
        return 87;
    case SkFontStyle::kSemiExpanded_Width:
        return 113;
    case SkFontStyle::kExpanded_Width:
        return 125;
    case SkFontStyle::kExtraExpanded_Width:
        return 150;
    case SkFontStyle::kUltraExpanded_Width:
        return 200;
    default:
        return 100;
    }
}

int SkFontMgrMlx::fontconfigSlant(const SkFontStyle& style)
{
    switch (style.slant()) {
    case SkFontStyle::kItalic_Slant:
        return 100;
    case SkFontStyle::kOblique_Slant:
        return 110;
    default:
        return 0;
    }
}

sk_sp<SkTypeface> SkFontMgrMlx::typefaceForFound(uintptr_t found) const
{
    if (!found)
        return nullptr;
    const char* path = mlxweb_font_path(found);
    int index = mlxweb_font_index(found);
    auto key = makeString(String::fromUTF8(path), '#', index);
    {
        Locker locker { m_lock };
        if (auto typeface = m_typefaces.get(key)) {
            mlxweb_font_release(found);
            return typeface;
        }
    }
    auto typeface = onMakeFromFile(path, index);
    mlxweb_font_release(found);
    if (!typeface)
        return nullptr;
    Locker locker { m_lock };
    m_typefaces.set(key, typeface);
    return typeface;
}

// No family sets: WebCore asks for a family's style (matchFamilyStyle).
sk_sp<SkFontStyleSet> SkFontMgrMlx::onMatchFamily(const char[]) const
{
    return SkFontStyleSet::CreateEmpty();
}

sk_sp<SkTypeface> SkFontMgrMlx::onMatchFamilyStyle(const char familyName[], const SkFontStyle& style) const
{
    return typefaceForFound(mlxweb_font_match(familyName && *familyName ? familyName : nullptr, style.weight(), fontconfigWidth(style), fontconfigSlant(style)));
}

sk_sp<SkTypeface> SkFontMgrMlx::onMatchFamilyStyleCharacter(const char*, const SkFontStyle& style, const char* bcp47[], int bcp47Count, SkUnichar character) const
{
    // (The most specific language comes last.)
    const char* locale = bcp47Count > 0 ? bcp47[bcp47Count - 1] : nullptr;
    return typefaceForFound(mlxweb_font_fallback(character, locale, style.weight(), fontconfigWidth(style), fontconfigSlant(style)));
}

sk_sp<SkTypeface> SkFontMgrMlx::onMakeFromData(sk_sp<SkData> data, int ttcIndex) const
{
    return onMakeFromStreamIndex(SkMemoryStream::Make(std::move(data)), ttcIndex);
}

sk_sp<SkTypeface> SkFontMgrMlx::onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset> stream, int ttcIndex) const
{
    return onMakeFromStreamArgs(std::move(stream), SkFontArguments().setCollectionIndex(ttcIndex));
}

sk_sp<SkTypeface> SkFontMgrMlx::onMakeFromStreamArgs(std::unique_ptr<SkStreamAsset> stream, const SkFontArguments& arguments) const
{
    if (!stream)
        return nullptr;
    return SkTypeface_FreeType::MakeFromStream(std::move(stream), arguments);
}

sk_sp<SkTypeface> SkFontMgrMlx::onMakeFromFile(const char path[], int ttcIndex) const
{
    return onMakeFromStreamIndex(SkStream::MakeFromFile(path), ttcIndex);
}

sk_sp<SkTypeface> SkFontMgrMlx::onLegacyMakeTypeface(const char familyName[], SkFontStyle style) const
{
    if (auto typeface = onMatchFamilyStyle(familyName, style))
        return typeface;
    return onMatchFamilyStyle(nullptr, style);
}

} // namespace WebCore
