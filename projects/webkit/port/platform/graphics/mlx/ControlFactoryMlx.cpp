// The Mlx port's form controls (PORT.md, "The theme"): ControlFactory's
// parts drawn as std.ui draws its widgets, in std.ui.theme's light or dark
// palette (MlxThemeColors.h) as the page's scheme asks: buttons and
// selects (menu lists) of the button colour, lit under the pointer, with
// a chevron; check boxes and radio buttons of the control colours, of the
// accent when on; text fields of the field colour in a control-coloured
// outline (the accent's, two pixels, with the focus); progress bars,
// slider tracks of a switch's off colour filled with the accent, a white
// knob; spin buttons. The geometry is RenderThemeAdwaita's (sizes and
// rectangles as WebKit's Adwaita controls have them), so layout is as
// before. ControlFactory::create() is the port's: WebKit's Adwaita
// control parts are not compiled (lists/webcore-mlx-skipped.txt).
#include "config.h"
#include "ControlFactoryMlx.h"

#include "Adwaita.h"
#include "ButtonPart.h"
#include "ColorBlending.h"
#include "ColorWellPart.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "GraphicsContextStateSaver.h"
#include "InnerSpinButtonPart.h"
#include "MenuListButtonPart.h"
#include "MenuListPart.h"
#include "MlxThemeColors.h"
#include "NotImplemented.h"
#include "Path.h"
#include "PlatformControl.h"
#include "ProgressBarPart.h"
#include "SearchFieldPart.h"
#include "SliderThumbPart.h"
#include "SliderTrackPart.h"
#include "TextAreaPart.h"
#include "TextFieldPart.h"
#include "ToggleButtonPart.h"

namespace WebCore {

namespace {

using State = ControlStyle::State;

bool isDark(const ControlStyle& style)
{
    return style.states.contains(State::DarkAppearance);
}

Color palette(MlxThemeColor which, const ControlStyle& style)
{
    return mlxThemeColor(which, isDark(style));
}

// The page's accent-color, else the desktop's accent.
Color accentOf(const ControlStyle& style)
{
    return style.accentColor.isValid() ? style.accentColor : palette(MlxThemeColor::Accent, style);
}

bool isEnabled(const ControlStyle& style)
{
    return style.states.contains(State::Enabled);
}

bool isLit(const ControlStyle& style)
{
    return isEnabled(style) && style.states.contains(State::Hovered);
}

void fillRounded(GraphicsContext& context, const FloatRect& rect, float radius, const Color& color)
{
    radius = std::max(0.f, std::min(radius, std::min(rect.width(), rect.height()) / 2));
    Path path;
    path.addRoundedRect(rect, { radius, radius });
    context.setFillRule(WindRule::NonZero);
    context.setFillColor(color);
    context.fillPath(path);
}

void fillEllipse(GraphicsContext& context, const FloatRect& rect, const Color& color)
{
    Path path;
    path.addEllipseInRect(rect);
    context.setFillRule(WindRule::NonZero);
    context.setFillColor(color);
    context.fillPath(path);
}

// A ring of WIDTH inside RECT (rounded by RADIUS).
void strokeRounded(GraphicsContext& context, const FloatRect& rect, float radius, float width, const Color& color)
{
    radius = std::max(0.f, std::min(radius, std::min(rect.width(), rect.height()) / 2));
    Path path;
    path.addRoundedRect(rect, { radius, radius });
    FloatRect inner = rect;
    inner.inflate(-width);
    float innerRadius = std::max(0.f, radius - width);
    path.addRoundedRect(inner, { innerRadius, innerRadius });
    context.setFillRule(WindRule::EvenOdd);
    context.setFillColor(color);
    context.fillPath(path);
    context.setFillRule(WindRule::NonZero);
}

// A chevron pointing down (or up) in the middle of RECT, as std.ui's
// menu buttons have it.
void paintChevron(GraphicsContext& context, const FloatRect& rect, bool up, const Color& color, float zoom)
{
    float size = std::min(rect.width(), rect.height());
    float half = size * 0.25f;
    FloatPoint center = rect.center();
    float rise = up ? -half / 2 : half / 2;
    Path path;
    path.moveTo({ center.x() - half, center.y() - rise });
    path.addLineTo({ center.x(), center.y() + rise });
    path.addLineTo({ center.x() + half, center.y() - rise });
    GraphicsContextStateSaver saver(context);
    context.setStrokeThickness(1.5f * zoom);
    context.setLineCap(LineCap::Round);
    context.setLineJoin(LineJoin::Round);
    context.setStrokeColor(color);
    context.strokePath(path);
}

// A control drawn at half its opacity when disabled, as Adwaita's are.
class DisabledLayer {
public:
    DisabledLayer(GraphicsContext& context, bool disabled)
        : m_context(context)
        , m_disabled(disabled)
    {
        if (m_disabled)
            m_context.beginTransparencyLayer(Adwaita::disabledOpacity);
    }
    ~DisabledLayer()
    {
        if (m_disabled)
            m_context.endTransparencyLayer();
    }

private:
    GraphicsContext& m_context;
    bool m_disabled;
};

// Buttons and colour wells: the button colour (lit under the pointer,
// darker held), rounded as std.ui's buttons.
void drawButton(GraphicsContext& context, const FloatRoundedRect& borderRect, const ControlStyle& style)
{
    GraphicsContextStateSaver saver(context);
    DisabledLayer layer(context, !isEnabled(style));
    MlxThemeColor fill = MlxThemeColor::Button;
    if (isEnabled(style) && style.states.contains(State::Pressed))
        fill = MlxThemeColor::Pressed;
    else if (isLit(style))
        fill = MlxThemeColor::ButtonHover;
    fillRounded(context, borderRect.rect(), 6 * style.zoomFactor, palette(fill, style));
    if (style.states.contains(State::Focused))
        Adwaita::paintFocus(context, borderRect.rect(), Adwaita::buttonFocusOffset, Adwaita::focusColor(accentOf(style)));
}

class ButtonControlMlx final : public PlatformControl {
public:
    using PlatformControl::PlatformControl;
    void draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float, const ControlStyle& style) final { drawButton(context, borderRect, style); }
};

// A select: a button with a chevron at its end.
class MenuListMlx final : public PlatformControl {
public:
    using PlatformControl::PlatformControl;
    void draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float, const ControlStyle& style) final
    {
        drawButton(context, borderRect, style);
        float arrow = Adwaita::menuListButtonArrowSize * style.zoomFactor;
        FloatRect rect = borderRect.rect();
        FloatRect arrowRect { rect.location(), FloatSize { arrow, arrow } };
        if (!style.states.contains(State::VerticalWritingMode)) {
            float y = (rect.height() - arrow) / 2;
            if (style.states.contains(State::InlineFlippedWritingMode))
                arrowRect.move(Adwaita::menuListButtonPadding, y);
            else
                arrowRect.move(rect.width() - arrow - Adwaita::menuListButtonPadding, y);
        } else
            arrowRect.move((rect.width() - arrow) / 2, rect.height() - arrow - Adwaita::menuListButtonPadding);
        DisabledLayer layer(context, !isEnabled(style));
        paintChevron(context, arrowRect, false, palette(MlxThemeColor::Dim, style), style.zoomFactor);
    }
};

// Text fields, text areas, search fields: the field colour in an outline
// of the control colour, the accent's (two pixels) with the focus.
class TextFieldMlx final : public PlatformControl {
public:
    using PlatformControl::PlatformControl;
    void draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float, const ControlStyle& style) final
    {
        GraphicsContextStateSaver saver(context);
        bool enabled = isEnabled(style) && !style.states.contains(State::ReadOnly);
        DisabledLayer layer(context, !enabled);
        bool focused = enabled && style.states.contains(State::Focused);
        FloatRect rect = borderRect.rect();
        float radius = 5;
        fillRounded(context, rect, radius, palette(isDark(style) ? MlxThemeColor::Raised : MlxThemeColor::Field, style));
        strokeRounded(context, rect, radius, focused ? 2 : 1, focused ? accentOf(style) : palette(MlxThemeColor::Control, style));
        if (style.states.contains(State::ListButton)) {
            float arrow = Adwaita::menuListButtonArrowSize * style.zoomFactor;
            FloatRect arrowRect { rect.location(), FloatSize { arrow, arrow } };
            if (!style.states.contains(State::VerticalWritingMode)) {
                float y = (rect.height() - arrow) / 2;
                if (style.states.contains(State::InlineFlippedWritingMode))
                    arrowRect.move(Adwaita::textFieldBorderSize, y);
                else
                    arrowRect.move(rect.width() - arrow + Adwaita::textFieldBorderSize, y);
            } else
                arrowRect.move((rect.width() - arrow) / 2, rect.height() - arrow + Adwaita::textFieldBorderSize);
            paintChevron(context, arrowRect, false, palette(MlxThemeColor::Dim, style), style.zoomFactor);
        }
    }
};

// Check boxes and radio buttons as std.ui's: off, the control colour
// round the inner one; on, the accent with the mark (a check, a dash for
// mixed, a dot) in the colour on it.
class ToggleButtonMlx final : public PlatformControl {
public:
    using PlatformControl::PlatformControl;
    void draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float, const ControlStyle& style) final
    {
        GraphicsContextStateSaver saver(context);
        DisabledLayer layer(context, !isEnabled(style));
        FloatRect rect = borderRect.rect();
        // (Square, in the middle of what is given.)
        float size = std::min(rect.width(), rect.height());
        rect = { rect.x() + (rect.width() - size) / 2, rect.y() + (rect.height() - size) / 2, size, size };
        bool checkbox = m_owningPart.get()->type() == StyleAppearance::Checkbox;
        bool on = style.states.containsAny({ State::Checked, State::Indeterminate });
        float unit = size / 14;
        Color accent = accentOf(style);
        if (isLit(style))
            accent = blendSourceOver(accent, Color(Color::white).colorWithAlphaMultipliedBy(0.12));
        if (checkbox) {
            if (on) {
                fillRounded(context, rect, 4 * unit, accent);
                Path mark;
                if (style.states.contains(State::Indeterminate))
                    mark.addRoundedRect(FloatRect(rect.x() + 3 * unit, rect.y() + 6 * unit, 8 * unit, 2 * unit), { unit, unit });
                else {
                    // (Adwaita's check, in the 14 units of the box.)
                    auto at = [&](float x, float y) { return FloatPoint { rect.x() + x * unit, rect.y() + y * unit }; };
                    mark.moveTo(at(2.43, 6.57));
                    mark.addLineTo(at(5.8, 9.94));
                    mark.addLineTo(at(11.8, 3.8));
                    mark.addLineTo(at(11.8, 5.9));
                    mark.addLineTo(at(5.8, 12.06));
                    mark.addLineTo(at(2.43, 8.69));
                    mark.closeSubpath();
                }
                context.setFillColor(palette(MlxThemeColor::OnAccent, style));
                context.fillPath(mark);
            } else {
                fillRounded(context, rect, 4 * unit, palette(isLit(style) ? MlxThemeColor::Border : MlxThemeColor::Control, style));
                FloatRect inner = rect;
                inner.inflate(-unit);
                fillRounded(context, inner, 3 * unit, palette(MlxThemeColor::ControlInner, style));
            }
        } else {
            if (on) {
                fillEllipse(context, rect, accent);
                fillEllipse(context, { rect.x() + 4 * unit, rect.y() + 4 * unit, 6 * unit, 6 * unit }, palette(MlxThemeColor::OnAccent, style));
            } else {
                fillEllipse(context, rect, palette(isLit(style) ? MlxThemeColor::Border : MlxThemeColor::Control, style));
                FloatRect inner = rect;
                inner.inflate(-unit);
                fillEllipse(context, inner, palette(MlxThemeColor::ControlInner, style));
            }
        }
        if (style.states.contains(State::Focused))
            Adwaita::paintFocus(context, borderRect.rect(), Adwaita::toggleFocusOffset, Adwaita::focusColor(accentOf(style)), checkbox ? Adwaita::PaintRounded::No : Adwaita::PaintRounded::Yes);
    }
};

// A progress bar: a switch's off colour, the accent as far as it has come
// (moving to and fro when it does not say how far).
class ProgressBarMlx final : public PlatformControl {
public:
    using PlatformControl::PlatformControl;
    void draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float, const ControlStyle& style) final
    {
        GraphicsContextStateSaver saver(context);
        FloatRect rect = borderRect.rect();
        bool vertical = style.states.contains(State::VerticalWritingMode);
        float thickness = vertical ? rect.width() : rect.height();
        fillRounded(context, rect, thickness / 2, palette(MlxThemeColor::SwitchOff, style));
        Ref part = downcast<ProgressBarPart>(m_owningPart.get());
        float length = vertical ? rect.height() : rect.width();
        float start = 0;
        float filled;
        if (part->position() >= 0) {
            filled = length * part->position();
            if (style.states.contains(State::InlineFlippedWritingMode))
                start = length - filled;
        } else {
            double duration = Adwaita::progressAnimationDuration.seconds();
            double progress = fmod((MonotonicTime::now().secondsSinceEpoch() - part->animationStartTime()).seconds(), duration) / duration;
            filled = std::max<float>(2, length / Adwaita::progressActivityBlocks);
            start = (progress < 0.5 ? progress * 2 : (1 - progress) * 2) * (length - filled);
        }
        FloatRect bar = rect;
        if (vertical) {
            bar.move(0, start);
            bar.setHeight(filled);
        } else {
            bar.move(start, 0);
            bar.setWidth(filled);
        }
        if (filled > 0)
            fillRounded(context, bar, thickness / 2, accentOf(style));
    }
};

// A slider's track: the switch's off colour (lit under the pointer), the
// accent up to the knob; its ticks.
class SliderTrackMlx final : public PlatformControl {
public:
    using PlatformControl::PlatformControl;
    void draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float, const ControlStyle& style) final
    {
        Ref part = downcast<SliderTrackPart>(m_owningPart.get());
        GraphicsContextStateSaver saver(context);
        DisabledLayer layer(context, !isEnabled(style));
        FloatRect rect = borderRect.rect();
        FloatRect track = rect;
        bool horizontal = part->type() == StyleAppearance::SliderHorizontal;
        float size = Adwaita::sliderTrackSize;
        if (horizontal) {
            track.move(0, rect.height() / 2 - size / 2);
            track.setHeight(size);
        } else {
            track.move(rect.width() / 2 - size / 2, 0);
            track.setWidth(size);
        }
        fillRounded(context, track, size / 2, palette(isLit(style) ? MlxThemeColor::SwitchOffHover : MlxThemeColor::SwitchOff, style));
        FloatRect range = track;
        if (horizontal) {
            float offset = range.width() * part->thumbPosition();
            if (style.states.contains(State::InlineFlippedWritingMode))
                range.move(range.width() - offset, 0);
            range.setWidth(offset);
        } else {
            float offset = range.height() * part->thumbPosition();
            if (!style.states.contains(State::VerticalWritingMode))
                range.move(0, range.height() - offset);
            range.setHeight(offset);
        }
        if (!range.isEmpty())
            fillRounded(context, range, size / 2, accentOf(style));
        part->drawTicks(context, rect, style);
        if (style.states.contains(State::Focused))
            Adwaita::paintFocus(context, track, Adwaita::sliderTrackFocusOffset, Adwaita::focusColor(accentOf(style)), Adwaita::PaintRounded::Yes);
    }
};

// A slider's knob: std.ui's white knob in a ring of the control colour
// (the accent's while held).
class SliderThumbMlx final : public PlatformControl {
public:
    using PlatformControl::PlatformControl;
    void draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float, const ControlStyle& style) final
    {
        GraphicsContextStateSaver saver(context);
        DisabledLayer layer(context, !isEnabled(style));
        FloatRect rect = borderRect.rect();
        bool held = isEnabled(style) && style.states.contains(State::Pressed);
        fillEllipse(context, rect, held ? accentOf(style) : palette(isLit(style) ? MlxThemeColor::Border : MlxThemeColor::Control, style));
        FloatRect inner = rect;
        inner.inflate(held ? -2 : -1);
        fillEllipse(context, inner, palette(MlxThemeColor::Knob, style));
    }
};

// A number field's spin buttons: the field colour in the control
// outline, the half under the pointer lit, chevrons.
class InnerSpinButtonMlx final : public PlatformControl {
public:
    using PlatformControl::PlatformControl;
    void draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float, const ControlStyle& style) final
    {
        GraphicsContextStateSaver saver(context);
        FloatRect rect = borderRect.rect();
        fillRounded(context, rect, 2, palette(MlxThemeColor::Control, style));
        FloatRect inner = rect;
        inner.inflate(-Adwaita::buttonBorderSize);
        fillRounded(context, inner, 1, palette(isDark(style) ? MlxThemeColor::Raised : MlxThemeColor::Field, style));
        FloatRect upper = inner;
        upper.setHeight(inner.height() / 2);
        FloatRect lower = upper;
        lower.move(0, upper.height());
        bool up = style.states.contains(State::SpinUp);
        if (isEnabled(style) && style.states.containsAny({ State::Hovered, State::Pressed }))
            context.fillRect(up ? upper : lower, palette(style.states.contains(State::Pressed) ? MlxThemeColor::Pressed : MlxThemeColor::Hover, style));
        Color dim = palette(MlxThemeColor::Dim, style);
        paintChevron(context, upper, true, dim, style.zoomFactor);
        paintChevron(context, lower, false, dim, style.zoomFactor);
    }
};

} // namespace

Ref<ControlFactory> ControlFactory::create()
{
    return adoptRef(*new ControlFactoryMlx());
}

#if ENABLE(APPLE_PAY)
std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformApplePayButton(ApplePayButtonPart&)
{
    notImplemented();
    return nullptr;
}
#endif

std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformButton(ButtonPart& part)
{
    return makeUnique<ButtonControlMlx>(part);
}

std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformColorWell(ColorWellPart& part)
{
    return makeUnique<ButtonControlMlx>(part);
}

#if ENABLE(SERVICE_CONTROLS)
std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformImageControlsButton(ImageControlsButtonPart&)
{
    notImplemented();
    return nullptr;
}
#endif

std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformInnerSpinButton(InnerSpinButtonPart& part)
{
    return makeUnique<InnerSpinButtonMlx>(part);
}

std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformMenuList(MenuListPart& part)
{
    return makeUnique<MenuListMlx>(part);
}

std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformMenuListButton(MenuListButtonPart& part)
{
    return makeUnique<MenuListMlx>(part);
}

std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformMeter(MeterPart&)
{
    // (RenderThemeAdwaita has no supportsMeter: a meter is its shadow
    // tree, coloured by RenderThemeMlx.cpp's style sheet.)
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformProgressBar(ProgressBarPart& part)
{
    return makeUnique<ProgressBarMlx>(part);
}

std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformSearchField(SearchFieldPart& part)
{
    return makeUnique<TextFieldMlx>(part);
}

std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformSearchFieldCancelButton(SearchFieldCancelButtonPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformSearchFieldResults(SearchFieldResultsPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformSliderThumb(SliderThumbPart& part)
{
    return makeUnique<SliderThumbMlx>(part);
}

std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformSliderTrack(SliderTrackPart& part)
{
    return makeUnique<SliderTrackMlx>(part);
}

std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformSwitch(SwitchPart&)
{
    notImplemented();
    return nullptr;
}

std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformTextArea(TextAreaPart& part)
{
    return makeUnique<TextFieldMlx>(part);
}

std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformTextField(TextFieldPart& part)
{
    return makeUnique<TextFieldMlx>(part);
}

std::unique_ptr<PlatformControl> ControlFactoryMlx::createPlatformToggleButton(ToggleButtonPart& part)
{
    return makeUnique<ToggleButtonMlx>(part);
}

} // namespace WebCore
