// The Mlx port's FontCache platform functions (PORT.md, "Fonts"): what
// FontCacheSkia.cpp defines of FontCache for GTK and WPE, which the port
// does not compile (it calls Fontconfig's C API), over the port's font
// manager (SkFontMgrMlx: std.fontconfig's matching, Skia's FreeType
// typefaces); and SkiaSystemFallbackFontCache, the fallback fonts of
// characters, whose header FontCache.h includes (its
// SkiaSystemFallbackFontCache.cpp calls Fontconfig too).
#include "config.h"
#include "FontCache.h"

#include "Font.h"
#include "FontCustomPlatformData.h"
#include "FontDescription.h"
#include "SkFontMgrMlx.h"
#include "SkiaSystemFallbackFontCache.h"
#include "WebKitFontFamilyNames.h"
#include "port/MlxWebPort.h"
#include <wtf/text/CString.h>
#include <wtf/text/CharacterProperties.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

void FontCache::platformInit()
{
}

SkFontMgr& FontCache::fontManager() const
{
    if (!m_fontManager)
        m_fontManager = SkFontMgrMlx::create();
    return *m_fontManager.get();
}

// A FontDescription's style as Skia's (CSS weights, widths and slants).
static SkFontStyle skiaFontStyle(const FontDescription& description)
{
    int weight = SkFontStyle::kNormal_Weight;
    auto descriptionWeight = description.weight();
    if (descriptionWeight > FontSelectionValue(SkFontStyle::kInvisible_Weight) && descriptionWeight <= FontSelectionValue(SkFontStyle::kExtraBlack_Weight))
        weight = static_cast<int>(descriptionWeight);

    // The widest width the description's is at least, from the narrowest.
    struct WidthStep {
        FontSelectionValue atLeast;
        SkFontStyle::Width width;
    };
    const WidthStep steps[] = {
        { extraCondensedWidthValue(), SkFontStyle::kExtraCondensed_Width },
        { condensedWidthValue(), SkFontStyle::kCondensed_Width },
        { semiCondensedWidthValue(), SkFontStyle::kSemiCondensed_Width },
        { normalWidthValue(), SkFontStyle::kNormal_Width },
        { semiExpandedWidthValue(), SkFontStyle::kSemiExpanded_Width },
        { expandedWidthValue(), SkFontStyle::kExpanded_Width },
        { extraExpandedWidthValue(), SkFontStyle::kExtraExpanded_Width },
        { ultraExpandedWidthValue(), SkFontStyle::kUltraExpanded_Width },
    };
    int width = SkFontStyle::kUltraCondensed_Width;
    for (auto& step : steps) {
        if (description.width() >= step.atLeast)
            width = step.width;
    }

    auto slant = SkFontStyle::kUpright_Slant;
    if (auto slope = description.fontStyleSlope()) {
        if (*slope > italicThreshold())
            slant = SkFontStyle::kOblique_Slant;
        else if (*slope > normalItalicValue())
            slant = SkFontStyle::kItalic_Slant;
    }
    return SkFontStyle(weight, width, slant);
}

// Whether a bold or an oblique face is made of the font's regular one (the
// font has no bold face, no italic face): not for color fonts.
static std::pair<bool, bool> syntheticBoldAndOblique(const SkTypeface& typeface, const FontDescription& description, OptionSet<FontLookupOptions> options)
{
    if (FontPlatformData::skiaTypefaceHasAnySupportedColorTable(typeface))
        return { false, false };
    bool bold = description.hasAutoFontSynthesisWeight() && !options.contains(FontLookupOptions::DisallowBoldSynthesis)
        && isFontWeightBold(description.weight()) && !typeface.isBold();
    bool oblique = description.allowsItalicOrObliqueFontSynthesisStyle() && !options.contains(FontLookupOptions::DisallowObliqueSynthesis)
        && isItalic(description.fontStyleSlope()) && !typeface.isItalic();
    return { bold, oblique };
}

static FontPlatformData platformDataFor(sk_sp<SkTypeface>&& typeface, const FontDescription& description, float size, OptionSet<FontLookupOptions> options, Vector<hb_feature_t>&& features)
{
    auto [syntheticBold, syntheticOblique] = syntheticBoldAndOblique(*typeface, description, options);
    return FontPlatformData(WTF::move(typeface), size, syntheticBold, syntheticOblique, description.orientation(), description.widthVariant(), description.textRenderingMode(), WTF::move(features));
}

RefPtr<Font> FontCache::systemFallbackForCharacterCluster(const FontDescription& description, const Font&, IsForPlatformFont, PreferColoredFont preferColoredFont, StringView stringView)
{
    auto codePoints = stringView.codePoints();
    auto iterator = codePoints.begin();
    char32_t character = *iterator;
    ++iterator;
    if (isDefaultIgnorableCodePoint(character) || isPrivateUseAreaCharacter(character))
        return nullptr;

    // Emoji presentation asks for a color font: Unicode's emoji locale.
    bool isEmoji = (iterator != codePoints.end() && *iterator == emojiVariationSelector) || preferColoredFont == PreferColoredFont::Yes;
    auto typeface = m_skiaSystemFallbackFontCache.fontForCharacterCluster(isEmoji ? "und-Zsye"_s : description.computedLocale(), stringView);
    if (!typeface)
        return nullptr;

    // font-size-adjust applies to fallback fonts, @font-face's size-adjust does not.
    auto size = description.computedSize();
    auto platformData = platformDataFor(WTF::move(typeface), description, size, { }, computeFeatures(description, { }));
    platformData.updateSizeWithFontSizeAdjust(description.fontSizeAdjust(), size);
    return fontForPlatformData(platformData);
}

// The families of the system's fonts (for the inspector): not listed yet.
Vector<String> FontCache::systemFontFamilies()
{
    return { };
}

bool FontCache::isSystemFontForbiddenForEditing(const String&)
{
    return false;
}

Ref<Font> FontCache::lastResortFallbackFont(const FontDescription& description)
{
    if (RefPtr font = fontForFamily(description, "serif"_s))
        return font.releaseNonNull();

    // Any font at all; else Skia's empty one, with no glyphs.
    auto typeface = fontManager().matchFamilyStyle(nullptr, skiaFontStyle(description));
    if (!typeface)
        typeface = SkTypeface::MakeEmpty();
    return fontForPlatformData(platformDataFor(WTF::move(typeface), description, description.computedSize(), { }, computeFeatures(description, { })));
}

Vector<FontSelectionCapabilities> FontCache::getFontSelectionCapabilitiesInFamily(const AtomString&, AllowUserInstalledFonts)
{
    return { };
}

// The family a CSS family stands for in Fontconfig's names: WebKit's
// generic families ("-webkit-standard"...) as Fontconfig's aliases.
static String fontconfigFamily(const String& family)
{
    if (family.length() && !family.startsWith("-webkit-"_s))
        return family;

    using WebKitFontFamilyNames::FamilyNamesIndex;
    auto name = [](FamilyNamesIndex index) -> String {
        return *WebKitFontFamilyNames::familyNamesData->at(index);
    };
    if (family == name(FamilyNamesIndex::StandardFamily) || family == name(FamilyNamesIndex::SerifFamily))
        return "serif"_s;
    if (family == name(FamilyNamesIndex::SansSerifFamily))
        return "sans-serif"_s;
    if (family == name(FamilyNamesIndex::MonospaceFamily))
        return "monospace"_s;
    if (family == name(FamilyNamesIndex::CursiveFamily))
        return "cursive"_s;
    if (family == name(FamilyNamesIndex::FantasyFamily))
        return "fantasy"_s;
    if (family == name(FamilyNamesIndex::MathFamily))
        return "math"_s;
    return emptyString();
}

// The OpenType features of a font for the description (CSS Fonts 3, 7.2,
// "Feature precedence"): the @font-face rule's, then those of the
// font-variant properties, then those other properties imply (ligatures
// off for letter spacing or optimizeSpeed, the widths of East Asian
// text), then font-feature-settings'. HarfBuzz has liga, clig and calt on
// and dlig and hlig off by default; only changes are given.
Vector<hb_feature_t> FontCache::computeFeatures(const FontDescription& description, const FontCreationContext& creationContext)
{
    FeaturesMap features;
    auto set = [&](std::span<const char, 5> tag, int value) {
        features.set(fontFeatureTag(tag), value);
    };

    if (creationContext.fontFaceFeatures()) {
        for (auto& feature : *creationContext.fontFaceFeatures())
            features.set(feature.tag(), feature.value());
    }

    for (auto& feature : computeFeatureSettingsFromVariants(description.variantSettings(), creationContext.fontFeatureValues()))
        features.set(feature.key, feature.value);

    bool optimizeSpeed = description.textRenderingMode() == TextRenderingMode::OptimizeSpeed;
    bool spacing = description.shouldDisableLigaturesForSpacing();
    auto offByDefault = [&](FontVariantLigatures ligatures) {
        return spacing || ligatures == FontVariantLigatures::No || (ligatures == FontVariantLigatures::Normal && optimizeSpeed);
    };
    if (offByDefault(description.variantCommonLigatures())) {
        set("liga", 0);
        set("clig", 0);
    }
    if (!spacing && description.variantDiscretionaryLigatures() == FontVariantLigatures::Yes)
        set("dlig", 1);
    if (!spacing && description.variantHistoricalLigatures() == FontVariantLigatures::Yes)
        set("hlig", 1);
    if (offByDefault(description.variantContextualAlternates()))
        set("calt", 0);

    switch (description.widthVariant()) {
    case FontWidthVariant::RegularWidth: break;
    case FontWidthVariant::HalfWidth: set("hwid", 1); break;
    case FontWidthVariant::ThirdWidth: set("twid", 1); break;
    case FontWidthVariant::QuarterWidth: set("qwid", 1); break;
    }

    switch (description.variantEastAsianVariant()) {
    case FontVariantEastAsianVariant::Normal: break;
    case FontVariantEastAsianVariant::Jis78: set("jp78", 1); break;
    case FontVariantEastAsianVariant::Jis83: set("jp83", 1); break;
    case FontVariantEastAsianVariant::Jis90: set("jp90", 1); break;
    case FontVariantEastAsianVariant::Jis04: set("jp04", 1); break;
    case FontVariantEastAsianVariant::Simplified: set("smpl", 1); break;
    case FontVariantEastAsianVariant::Traditional: set("trad", 1); break;
    }

    switch (description.variantEastAsianWidth()) {
    case FontVariantEastAsianWidth::Normal: break;
    case FontVariantEastAsianWidth::Full: set("fwid", 1); break;
    case FontVariantEastAsianWidth::Proportional: set("pwid", 1); break;
    }

    if (description.variantEastAsianRuby() == FontVariantEastAsianRuby::Yes)
        set("ruby", 1);

    switch (description.variantNumericFigure()) {
    case FontVariantNumericFigure::Normal: break;
    case FontVariantNumericFigure::LiningNumbers: set("lnum", 1); break;
    case FontVariantNumericFigure::OldStyleNumbers: set("onum", 1); break;
    }

    switch (description.variantNumericSpacing()) {
    case FontVariantNumericSpacing::Normal: break;
    case FontVariantNumericSpacing::ProportionalNumbers: set("pnum", 1); break;
    case FontVariantNumericSpacing::TabularNumbers: set("tnum", 1); break;
    }

    switch (description.variantNumericFraction()) {
    case FontVariantNumericFraction::Normal: break;
    case FontVariantNumericFraction::DiagonalFractions: set("frac", 1); break;
    case FontVariantNumericFraction::StackedFractions: set("afrc", 1); break;
    }

    if (description.variantNumericOrdinal() == FontVariantNumericOrdinal::Yes)
        set("ordn", 1);
    if (description.variantNumericSlashedZero() == FontVariantNumericSlashedZero::Yes)
        set("zero", 1);

    for (auto& feature : description.featureSettings())
        features.set(feature.tag(), feature.value());

    Vector<hb_feature_t> result;
    result.reserveInitialCapacity(features.size());
    for (auto& [tag, value] : features)
        result.append({ HB_TAG(tag[0], tag[1], tag[2], tag[3]), static_cast<uint32_t>(value), 0, static_cast<unsigned>(-1) });
    return result;
}

std::unique_ptr<FontPlatformData> FontCache::createFontPlatformData(const FontDescription& description, const AtomString& family, const FontCreationContext& creationContext, OptionSet<FontLookupOptions> options)
{
    // (Apple's system font names are no font here.)
    if (family == "-apple-system"_s || family == "-apple-system-font"_s)
        return nullptr;
    auto typeface = fontManager().matchFamilyStyle(fontconfigFamily(family).utf8().data(), skiaFontStyle(description));
    if (!typeface)
        return nullptr;

    auto size = description.adjustedSizeForFontFace(creationContext.sizeAdjust());
    auto platformData = platformDataFor(WTF::move(typeface), description, size, options, computeFeatures(description, creationContext));
    platformData.updateSizeWithFontSizeAdjust(description.fontSizeAdjust(), description.computedSize());
    return makeUnique<FontPlatformData>(platformData);
}

ASCIILiteral FontCache::platformAlternateFamilyName(const String&)
{
    return { };
}

void FontCache::platformReleaseNoncriticalMemory()
{
    for (auto& entry : m_fontCascadeCache.m_entries.values()) {
        entry->fonts->forEachRealizedFont([](const Font& font) {
            if (auto* customPlatformData = font.platformData().customPlatformData())
                customPlatformData->clearVariationTypefacesCache();
        });
    }
}

void FontCache::platformInvalidate()
{
}

void FontCache::platformPurgeInactiveFontData()
{
    m_harfBuzzFontCache.clear();
    m_skiaSystemFallbackFontCache.clear();
}

// The port's FontSetCache (SkiaSystemFallbackFontCache's cache of a
// locale's fallback fonts, whose class FreeType's FontSetCache.cpp, not
// compiled, defines on Fontconfig): the typefaces found for characters of
// one locale.
class FontSetCache {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(FontSetCache);
public:
    HashMap<char32_t, sk_sp<SkTypeface>, IntHash<char32_t>, WTF::UnsignedWithZeroKeyHashTraits<char32_t>> typefaces;
};

SkiaSystemFallbackFontCache::SkiaSystemFallbackFontCache() = default;

SkiaSystemFallbackFontCache::~SkiaSystemFallbackFontCache() = default;

sk_sp<SkTypeface> SkiaSystemFallbackFontCache::fontForCharacterCluster(const String& locale, StringView stringView)
{
    auto codePoints = stringView.codePoints();
    auto iterator = codePoints.begin();
    if (iterator == codePoints.end())
        return nullptr;
    char32_t character = *iterator;

    // (A null locale, no lang given, is the empty string's: a String key
    // of a HashMap is never null.)
    auto& cache = m_cache.ensure(locale.isNull() ? emptyString() : locale, [] {
        return makeUnique<FontSetCache>();
    }).iterator->value;
    if (auto found = cache->typefaces.find(character); found != cache->typefaces.end())
        return found->value;

    auto localeName = locale.utf8();
    SkFontStyle style;
    auto typeface = static_cast<SkFontMgrMlx&>(FontCache::forCurrentThread().fontManager()).typefaceForFound(mlxweb_font_fallback(character, localeName.length() ? localeName.data() : nullptr, style.weight(), SkFontMgrMlx::fontconfigWidth(style), SkFontMgrMlx::fontconfigSlant(style)));
    cache->typefaces.set(character, typeface);
    return typeface;
}

void SkiaSystemFallbackFontCache::clear()
{
    m_cache.clear();
    m_typefaceCache.clear();
}

} // namespace WebCore
