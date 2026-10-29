/*
 * Copyright (C) 2014, 2020 Igalia S.L.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// The Mlx port's RenderTheme (PORT.md, "The theme"): WebKit's
// RenderThemeAdwaita.cpp (its geometry, the controls it draws, its sizes,
// unchanged), with the desktop's colours: std.ui.theme's light and dark
// palettes (MlxThemeColors.h) for the system colours (Canvas, Field,
// ButtonFace, ButtonBorder, Highlight, SelectedItem, Mark, AccentColor...),
// the text selection (the accent, translucent) and the accent of the
// controls (ControlFactoryMlx.cpp draws them). RenderTheme.h declares
// RenderTheme::singleton() as a RenderThemeAdwaita, whose members are
// final: the port defines that class's members here, and its source list
// does not compile WebKit's RenderThemeAdwaita.cpp
// (lists/webcore-mlx-skipped.txt).


#include "config.h"
#include "RenderThemeAdwaita.h"

#if USE(THEME_ADWAITA)

#include "Adwaita.h"
#include "ColorSerialization.h"
#include "MlxThemeColors.h"
#include "port/MlxWebPort.h"
#include "Color.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "MediaControlTextTrackContainerElement.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderBox.h"
#include "RenderObjectInlines.h"
#include "RenderProgress.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StylePadding.h"
#include "ThemeAdwaita.h"
#include "TimeRanges.h"
#include "UserAgentScripts.h"
#include "UserAgentStyleSheets.h"
#include <wtf/text/Base64.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(WIN)
#include "WebCoreBundleWin.h"
#endif
#include "SharedBuffer.h"
#include <stdlib.h>
#include <wtf/FileSystem.h>
#include <wtf/text/MakeString.h>

#if PLATFORM(GTK) || PLATFORM(WPE)
#include "SystemSettings.h"
#endif

#if USE(GLIB)
#include <wtf/glib/GSpanExtras.h>
#endif

namespace WebCore {
using namespace CSS::Literals;
using namespace WebCore::Adwaita;

RenderThemeAdwaita& RenderTheme::singleton()
{
    static MainThreadNeverDestroyed<RenderThemeAdwaita> theme;
    // (The controls' accent and the focus rings: std.ui.theme's accent.)
    static bool accented = [] {
        static_cast<ThemeAdwaita&>(Theme::singleton()).setAccentColor(mlxThemeColor(MlxThemeColor::Accent, false));
        return true;
    }();
    UNUSED_VARIABLE(accented);
    return theme;
}

static Color paletteColor(uint32_t argb)
{
    return SRGBA<uint8_t> { static_cast<uint8_t>(argb >> 16), static_cast<uint8_t>(argb >> 8), static_cast<uint8_t>(argb), static_cast<uint8_t>(argb >> 24) };
}

Color mlxThemeColor(MlxThemeColor which, bool dark)
{
    // (The palettes are fixed: each colour asked of std.ui.theme once.)
    static std::array<std::array<std::optional<Color>, 32>, 2> palettes;
    auto& slot = palettes[dark ? 1 : 0][static_cast<size_t>(which)];
    if (!slot)
        slot = paletteColor(mlxweb_theme_color(dark ? 1 : 0, static_cast<uint32_t>(which)));
    return *slot;
}

Color mlxSelectionBackground()
{
    return mlxThemeColor(MlxThemeColor::Accent, false).colorWithAlpha(0.38);
}

RenderThemeAdwaita::~RenderThemeAdwaita() = default;

bool RenderThemeAdwaita::canCreateControlPartForRenderer(const RenderElement& renderer) const
{
    switch (renderer.style().usedAppearance()) {
    case StyleAppearance::Button:
    case StyleAppearance::Checkbox:
    case StyleAppearance::ColorWell:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::InnerSpinButton:
    case StyleAppearance::Menulist:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::PushButton:
    case StyleAppearance::Radio:
    case StyleAppearance::SearchField:
    case StyleAppearance::SliderThumbHorizontal:
    case StyleAppearance::SliderThumbVertical:
    case StyleAppearance::SliderHorizontal:
    case StyleAppearance::SliderVertical:
    case StyleAppearance::SquareButton:
        return true;
    default:
        break;
    }
    return false;
}

bool RenderThemeAdwaita::canCreateControlPartForBorderOnly(const RenderElement& renderer) const
{
    switch (renderer.style().usedAppearance()) {
    case StyleAppearance::Listbox:
    case StyleAppearance::TextArea:
    case StyleAppearance::TextField:
        return true;
    default:
        break;
    }
    return false;
}

bool RenderThemeAdwaita::canCreateControlPartForDecorations(const RenderElement& renderer) const
{
    return renderer.style().usedAppearance() == StyleAppearance::MenulistButton;
}

bool RenderThemeAdwaita::supportsFocusRing(const RenderElement&, const Style::ComputedStyle& style) const
{
    switch (style.usedAppearance()) {
    case StyleAppearance::PushButton:
    case StyleAppearance::Button:
    case StyleAppearance::TextField:
    case StyleAppearance::TextArea:
    case StyleAppearance::SearchField:
    case StyleAppearance::Menulist:
    case StyleAppearance::Radio:
    case StyleAppearance::Checkbox:
    case StyleAppearance::SliderHorizontal:
    case StyleAppearance::SliderVertical:
        return true;
    default:
        break;
    }

    return false;
}

bool RenderThemeAdwaita::shouldHaveCapsLockIndicator(const HTMLInputElement& element) const
{
    return element.isPasswordField();
}

// The selection as std.ui draws it (focused or not: a page's selection
// stays as visible as the desktop's).
Color RenderThemeAdwaita::platformActiveSelectionBackgroundColor(OptionSet<StyleColorOptions>) const
{
    return mlxSelectionBackground();
}

Color RenderThemeAdwaita::platformInactiveSelectionBackgroundColor(OptionSet<StyleColorOptions>) const
{
    return mlxSelectionBackground();
}

Color RenderThemeAdwaita::platformActiveSelectionForegroundColor(OptionSet<StyleColorOptions>) const
{
    return { };
}

Color RenderThemeAdwaita::platformInactiveSelectionForegroundColor(OptionSet<StyleColorOptions>) const
{
    return { };
}

Color RenderThemeAdwaita::platformActiveListBoxSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
    return platformActiveListBoxSelectionForegroundColor(options).colorWithAlpha(0.15);
}

Color RenderThemeAdwaita::platformInactiveListBoxSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
    return platformInactiveListBoxSelectionForegroundColor(options).colorWithAlpha(0.15);
}

Color RenderThemeAdwaita::platformActiveListBoxSelectionForegroundColor(OptionSet<StyleColorOptions> options) const
{
    return mlxThemeColor(MlxThemeColor::Text, options.contains(StyleColorOptions::UseDarkAppearance));
}

Color RenderThemeAdwaita::platformInactiveListBoxSelectionForegroundColor(OptionSet<StyleColorOptions> options) const
{
    return platformActiveListBoxSelectionForegroundColor(options);
}

Color RenderThemeAdwaita::platformFocusRingColor(OptionSet<StyleColorOptions>) const
{
    return systemFocusRingColor();
}

float RenderThemeAdwaita::platformFocusRingWidth() const
{
    return Adwaita::focusLineWidth;
}

void RenderThemeAdwaita::platformColorsDidChange()
{
    static_cast<ThemeAdwaita&>(Theme::singleton()).platformColorsDidChange();
    RenderTheme::platformColorsDidChange();
}

// Adwaita's style sheet, and a meter's bar and value in the desktop's
// colours (RenderThemeAdwaita.h has no supportsMeter: a meter is its
// shadow tree, whose own style sheet only an !important rule of the user
// agent's comes before).
String RenderThemeAdwaita::extraDefaultStyleSheet()
{
    static NeverDestroyed<String> sheet = [] {
        auto both = [](MlxThemeColor which) {
            StringBuilder value;
            value.append("light-dark("_s);
            value.append(serializationForCSS(mlxThemeColor(which, false)));
            value.append(", "_s);
            value.append(serializationForCSS(mlxThemeColor(which, true)));
            value.append(')');
            return value.toString();
        };
        StringBuilder css;
        css.append(String { StringImpl::createWithoutCopying(themeAdwaitaUserAgentStyleSheet) });
        css.append("\nmeter::-webkit-meter-bar { background: "_s);
        css.append(both(MlxThemeColor::SwitchOff));
        css.append(" !important; border-radius: 0.5em; }"_s);
        css.append("\nmeter::-webkit-meter-optimum-value { background: "_s);
        css.append(both(MlxThemeColor::Accent));
        css.append(" !important; border-radius: 0.5em; }"_s);
        css.append("\nmeter::-webkit-meter-suboptimum-value { background: light-dark(#e5a50a, #f6c344) !important; border-radius: 0.5em; }"_s);
        css.append("\nmeter::-webkit-meter-even-less-good-value { background: "_s);
        css.append(both(MlxThemeColor::Danger));
        css.append(" !important; border-radius: 0.5em; }\n"_s);
        return css.toString();
    }();
    return sheet.get();
}

#if ENABLE(VIDEO)
// The media controls (a video or audio element's `controls`): WebKit's
// modern media controls, their script and style sheet from the user
// agent bundles the build makes of Modules/modern-media-controls; their
// icons from files: the directory MLX_WEBKIT_MEDIA_ICONS names, else
// the source tree's images/adwaita under the working directory, else
// /usr/share/mlx/voyager/media-controls.
Vector<String, 2> RenderThemeAdwaita::mediaControlsScripts()
{
    return { StringImpl::createWithoutCopying(ModernMediaControlsJavaScript) };
}

Vector<String, 2> RenderThemeAdwaita::mediaControlsStyleSheets(const HTMLMediaElement&)
{
    if (m_mediaControlsStyleSheet.isEmpty())
        m_mediaControlsStyleSheet = StringImpl::createWithoutCopying(ModernMediaControlsUserAgentStyleSheet);
    return { m_mediaControlsStyleSheet };
}

static std::optional<Vector<uint8_t>> mediaControlsIconFile(const String& iconName, const String& iconType)
{
    auto fileName = makeString(iconName, '.', iconType);
    Vector<String, 3> directories;
    if (const char* given = getenv("MLX_WEBKIT_MEDIA_ICONS"))
        directories.append(String::fromUTF8(given));
    directories.append("projects/webkit/upstream/Source/WebCore/Modules/modern-media-controls/images/adwaita"_s);
    directories.append("/usr/share/mlx/voyager/media-controls"_s);
    for (auto& directory : directories) {
        if (auto data = FileSystem::readEntireFile(FileSystem::pathByAppendingComponent(directory, fileName)))
            return data;
    }
    return std::nullopt;
}

RefPtr<FragmentedSharedBuffer> RenderThemeAdwaita::mediaControlsImageDataForIconNameAndType(const String& iconName, const String& iconType)
{
    auto data = mediaControlsIconFile(iconName, iconType);
    if (!data)
        return nullptr;
    return SharedBuffer::create(WTF::move(*data));
}

String RenderThemeAdwaita::mediaControlsBase64StringForIconNameAndType(const String& iconName, const String& iconType)
{
    auto data = mediaControlsIconFile(iconName, iconType);
    if (!data)
        return emptyString();
    return base64EncodeToString(*data);
}

// m:ss, or h:mm:ss from an hour on.
String RenderThemeAdwaita::mediaControlsFormattedStringForDuration(double durationInSeconds)
{
    if (!std::isfinite(durationInSeconds) || durationInSeconds < 0)
        return "0:00"_s;
    auto total = static_cast<uint64_t>(durationInSeconds);
    auto seconds = total % 60;
    auto minutes = (total / 60) % 60;
    auto hours = total / 3600;
    StringBuilder text;
    if (hours) {
        text.append(hours, ':');
        if (minutes < 10)
            text.append('0');
    }
    text.append(minutes, ':');
    if (seconds < 10)
        text.append('0');
    text.append(seconds);
    return text.toString();
}
#endif // ENABLE(VIDEO)


// The system colours of std.ui.theme's palette for the scheme (light: the
// canvas white and its text black, as pages expect them; dark: the
// desktop's background and text).
Color RenderThemeAdwaita::systemColor(CSSValueID cssValueID, OptionSet<StyleColorOptions> options) const
{
    const bool dark = options.contains(StyleColorOptions::UseDarkAppearance);
    auto palette = [dark](MlxThemeColor which) -> Color { return { mlxThemeColor(which, dark).toColorTypeLossy<SRGBA<uint8_t>>(), Color::Flags::Semantic }; };

    switch (cssValueID) {
    case CSSValueCanvastext:
    case CSSValueFieldtext:
        // (Black on a light page, as pages and WebKit's tests expect.)
        if (!dark)
            return { Color::black, Color::Flags::Semantic };
        return palette(MlxThemeColor::Text);
    case CSSValueActivebuttontext:
    case CSSValueButtontext:
    case CSSValueMarktext:
    case CSSValueHighlighttext:
        return palette(MlxThemeColor::Text);
    case CSSValueGraytext:
        return palette(MlxThemeColor::Dim);
    case CSSValueCanvas:
        return palette(dark ? MlxThemeColor::Background : MlxThemeColor::Raised);
    case CSSValueField:
        return palette(dark ? MlxThemeColor::Raised : MlxThemeColor::Field);
    case CSSValueButtonface:
        return palette(MlxThemeColor::Button);
    case CSSValueButtonborder:
        return palette(MlxThemeColor::Control);
    case CSSValueHighlight:
    case CSSValueMark:
        return palette(MlxThemeColor::Selection);
    case CSSValueSelecteditem:
    case CSSValueAccentcolor:
        return palette(MlxThemeColor::Accent);
    case CSSValueSelecteditemtext:
    case CSSValueAccentcolortext:
        return palette(MlxThemeColor::OnAccent);
    case CSSValueLinktext:
        if (dark)
            return palette(MlxThemeColor::Accent);
        break;
    default:
        break;
    }
    return RenderTheme::systemColor(cssValueID, options);
}

bool RenderThemeAdwaita::isControlStyled(const Style::ComputedStyle& style) const
{
    auto appearance = style.usedAppearance();
    if (appearance == StyleAppearance::TextField || appearance == StyleAppearance::TextArea || appearance == StyleAppearance::SearchField || appearance == StyleAppearance::Listbox)
        return style.nativeAppearanceDisabled();

    return RenderTheme::isControlStyled(style);
}

void RenderThemeAdwaita::adjustTextFieldStyle(Style::ComputedStyle& style, const Element*) const
{
    if (!style.hasExplicitlySetBorderRadius())
        style.setBorderRadius({ 5_css_px, 5_css_px });
}

void RenderThemeAdwaita::adjustTextAreaStyle(Style::ComputedStyle& style, const Element* element) const
{
    adjustTextFieldStyle(style, element);
}

void RenderThemeAdwaita::adjustSearchFieldStyle(Style::ComputedStyle& style, const Element* element) const
{
    adjustTextFieldStyle(style, element);
}

void RenderThemeAdwaita::adjustMenuListStyle(Style::ComputedStyle& style, const Element* element) const
{
    RenderTheme::adjustMenuListStyle(style, element);
    style.setLineHeight(Style::ComputedStyle::initialLineHeight());
}

void RenderThemeAdwaita::adjustMenuListButtonStyle(Style::ComputedStyle& style, const Element* element) const
{
    adjustMenuListStyle(style, element);
}

Style::PaddingBox RenderThemeAdwaita::platformPopupInternalPaddingBox(const Style::ComputedStyle& style) const
{
    if (style.usedAppearance() == StyleAppearance::None || style.usedAppearance() == StyleAppearance::Base)
        return { 0_css_px };

    auto zoomedArrowSize = menuListButtonArrowSize * style.usedZoom();

    return {
        Style::PaddingEdge::Fixed { static_cast<float>(menuListButtonPadding) },
        Style::PaddingEdge::Fixed { static_cast<float>(menuListButtonPadding + zoomedArrowSize) },
        Style::PaddingEdge::Fixed { static_cast<float>(menuListButtonPadding) },
        Style::PaddingEdge::Fixed { static_cast<float>(menuListButtonPadding) },
    };
}

Seconds RenderThemeAdwaita::animationRepeatIntervalForProgressBar(const RenderProgress& renderer) const
{
    return renderer.page().preferredRenderingUpdateInterval();
}

IntRect RenderThemeAdwaita::progressBarRectForBounds(const RenderProgress& renderer, const IntRect& bounds) const
{
    bool isHorizontal = renderer.isHorizontalWritingMode();
    return { bounds.x(), bounds.y(), isHorizontal ? bounds.width() : progressBarSize, isHorizontal ? progressBarSize : bounds.height() };
}

void RenderThemeAdwaita::adjustSliderThumbSize(Style::ComputedStyle& style, const Element*) const
{
    auto appearance = style.usedAppearance();
    if (appearance != StyleAppearance::SliderThumbHorizontal && appearance != StyleAppearance::SliderThumbVertical)
        return;

    style.setWidth(Style::PreferredSize::Fixed { static_cast<float>(sliderThumbSize) });
    style.setHeight(Style::PreferredSize::Fixed { static_cast<float>(sliderThumbSize) });
}

IntSize RenderThemeAdwaita::sliderTickSize() const
{
    return { 1, 7 };
}

int RenderThemeAdwaita::sliderTickOffsetFromTrackCenter() const
{
    return -16;
}

void RenderThemeAdwaita::adjustListButtonStyle(Style::ComputedStyle& style, const Element*) const
{
    style.setLogicalWidth(16_css_px);
    // Add a margin to place the button at end of the input field.
    style.setMarginEnd(-2_css_px);
}

Style::PreferredSizePair RenderThemeAdwaita::controlSize(StyleAppearance appearance, const FontCascade& fontCascade, const Style::PreferredSizePair& zoomedSize, float zoomFactor) const
{
    if (!zoomedSize.width().isSizingKeywordOrAuto() && !zoomedSize.height().isSizingKeywordOrAuto())
        return RenderTheme::controlSize(appearance, fontCascade, zoomedSize, zoomFactor);

    switch (appearance) {
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio: {
        auto buttonSizeWidth = zoomedSize.width();
        auto buttonSizeHeight = zoomedSize.height();
        if (buttonSizeWidth.isSizingKeywordOrAuto())
            buttonSizeWidth = 12_css_px * zoomFactor;
        if (buttonSizeHeight.isSizingKeywordOrAuto())
            buttonSizeHeight = 12_css_px * zoomFactor;
        return { WTF::move(buttonSizeWidth), WTF::move(buttonSizeHeight) };
    }
    case StyleAppearance::InnerSpinButton: {
        auto spinButtonSizeWidth = zoomedSize.width();
        auto spinButtonSizeHeight = zoomedSize.height();
        if (spinButtonSizeWidth.isSizingKeywordOrAuto())
            spinButtonSizeWidth = Style::PreferredSize::Fixed { static_cast<float>(static_cast<int>(arrowSize * zoomFactor)) };
        if (spinButtonSizeHeight.isSizingKeywordOrAuto() || fontCascade.size() > arrowSize)
            spinButtonSizeHeight = Style::PreferredSize::Fixed { fontCascade.size() };
        return { WTF::move(spinButtonSizeWidth), WTF::move(spinButtonSizeHeight) };
    }
    default:
        break;
    }

    return RenderTheme::controlSize(appearance, fontCascade, zoomedSize, zoomFactor);
}

Style::MinimumSizePair RenderThemeAdwaita::minimumControlSize(StyleAppearance, const FontCascade&, const Style::MinimumSizePair& zoomedSize, float) const
{
    if (!zoomedSize.width().isSizingKeywordOrAuto() && !zoomedSize.height().isSizingKeywordOrAuto())
        return zoomedSize;

    auto resultWidth = zoomedSize.width();
    auto resultHeight = zoomedSize.height();

    if (resultWidth.isSizingKeywordOrAuto())
        resultWidth = 0_css_px;
    if (resultHeight.isSizingKeywordOrAuto())
        resultHeight = 0_css_px;

    return { WTF::move(resultWidth), WTF::move(resultHeight) };
}

Style::LineWidthBox RenderThemeAdwaita::controlBorder(StyleAppearance appearance, const FontCascade& font, const Style::LineWidthBox& zoomedBox, float zoomFactor, const Element* element) const
{
    switch (appearance) {
    case StyleAppearance::PushButton:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button:
    case StyleAppearance::SquareButton:
        return zoomedBox;
    default:
        break;
    }

    return RenderTheme::controlBorder(appearance, font, zoomedBox, zoomFactor, element);
}

#if PLATFORM(GTK) || PLATFORM(WPE)
std::optional<Seconds> RenderThemeAdwaita::caretBlinkInterval() const
{
    auto shouldBlink = SystemSettings::singleton().cursorBlink();
    auto blinkTime = SystemSettings::singleton().cursorBlinkTime();
    if (shouldBlink.value_or(true))
        return { 500_us * blinkTime.value_or(1200) };
    return { };
}
#endif // PLATFORM(GTK) || PLATFORM(WPE)

void RenderThemeAdwaita::setAccentColor(const Color& color)
{
    static_cast<ThemeAdwaita&>(Theme::singleton()).setAccentColor(color);
    platformColorsDidChange();
}

} // namespace WebCore

#endif // USE(THEME_ADWAITA)
