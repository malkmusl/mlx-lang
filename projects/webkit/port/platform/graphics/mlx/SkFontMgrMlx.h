// The Mlx port's Skia font manager (PORT.md, "Fonts"): its fonts are the
// files std.fontconfig matches (through the port's Mlx side), opened as
// Skia's FreeType typefaces, in place of Skia's Fontconfig manager
// (SkFontMgr_fontconfig.cpp, on Fontconfig's C API, which the port does
// not have). Typefaces are kept by file and index, so a font is opened
// once.
#pragma once

#include <skia/core/SkFontMgr.h>
#include <skia/core/SkTypeface.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class SkFontMgrMlx final : public SkFontMgr {
public:
    static sk_sp<SkFontMgr> create();

    // The typeface of a font found by the Mlx side (mlxweb_font_match or
    // _fallback), released here.
    sk_sp<SkTypeface> typefaceForFound(uintptr_t found) const;

    // Fontconfig's width (FC_WIDTH_*) and slant (FC_SLANT_*) of a style.
    static int fontconfigWidth(const SkFontStyle&);
    static int fontconfigSlant(const SkFontStyle&);

protected:
    int onCountFamilies() const override { return 0; }
    void onGetFamilyName(int, SkString*) const override { }
    sk_sp<SkFontStyleSet> onCreateStyleSet(int) const override { return nullptr; }
    sk_sp<SkFontStyleSet> onMatchFamily(const char[]) const override;
    sk_sp<SkTypeface> onMatchFamilyStyle(const char familyName[], const SkFontStyle&) const override;
    sk_sp<SkTypeface> onMatchFamilyStyleCharacter(const char familyName[], const SkFontStyle&, const char* bcp47[], int bcp47Count, SkUnichar) const override;
    sk_sp<SkTypeface> onMakeFromData(sk_sp<SkData>, int ttcIndex) const override;
    sk_sp<SkTypeface> onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset>, int ttcIndex) const override;
    sk_sp<SkTypeface> onMakeFromStreamArgs(std::unique_ptr<SkStreamAsset>, const SkFontArguments&) const override;
    sk_sp<SkTypeface> onMakeFromFile(const char path[], int ttcIndex) const override;
    sk_sp<SkTypeface> onLegacyMakeTypeface(const char familyName[], SkFontStyle) const override;

private:
    mutable Lock m_lock;
    mutable HashMap<String, sk_sp<SkTypeface>> m_typefaces WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace WebCore
