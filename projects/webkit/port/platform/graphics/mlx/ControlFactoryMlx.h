// The Mlx port's ControlFactory (ControlFactoryMlx.cpp): form controls in
// std.ui.theme's colours. Its type is Adwaita's: the theme it serves is
// RenderThemeAdwaita's class (port/rendering/RenderThemeMlx.cpp).
#pragma once

#include "ControlFactory.h"

namespace WebCore {

class ControlFactoryMlx final : public ControlFactory {
private:
#if ENABLE(APPLE_PAY)
    std::unique_ptr<PlatformControl> createPlatformApplePayButton(ApplePayButtonPart&) final;
#endif
    std::unique_ptr<PlatformControl> createPlatformButton(ButtonPart&) final;
    std::unique_ptr<PlatformControl> createPlatformColorWell(ColorWellPart&) final;
#if ENABLE(SERVICE_CONTROLS)
    std::unique_ptr<PlatformControl> createPlatformImageControlsButton(ImageControlsButtonPart&) final;
#endif
    std::unique_ptr<PlatformControl> createPlatformInnerSpinButton(InnerSpinButtonPart&) final;
    std::unique_ptr<PlatformControl> createPlatformMenuList(MenuListPart&) final;
    std::unique_ptr<PlatformControl> createPlatformMenuListButton(MenuListButtonPart&) final;
    std::unique_ptr<PlatformControl> createPlatformMeter(MeterPart&) final;
    std::unique_ptr<PlatformControl> createPlatformProgressBar(ProgressBarPart&) final;
    std::unique_ptr<PlatformControl> createPlatformSearchField(SearchFieldPart&) final;
    std::unique_ptr<PlatformControl> createPlatformSearchFieldCancelButton(SearchFieldCancelButtonPart&) final;
    std::unique_ptr<PlatformControl> createPlatformSearchFieldResults(SearchFieldResultsPart&) final;
    std::unique_ptr<PlatformControl> createPlatformSliderThumb(SliderThumbPart&) final;
    std::unique_ptr<PlatformControl> createPlatformSliderTrack(SliderTrackPart&) final;
    std::unique_ptr<PlatformControl> createPlatformSwitch(SwitchPart&) final;
    std::unique_ptr<PlatformControl> createPlatformTextArea(TextAreaPart&) final;
    std::unique_ptr<PlatformControl> createPlatformTextField(TextFieldPart&) final;
    std::unique_ptr<PlatformControl> createPlatformToggleButton(ToggleButtonPart&) final;
    Type type() const final { return Type::Adwaita; }
};

} // namespace WebCore
