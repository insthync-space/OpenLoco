#include "Graphics/Colour.h"
#include "Graphics/Gfx.h"
#include "Graphics/SoftwareDrawingEngine.h"
#include "Graphics/TextRenderer.h"
#include "Input.h"
#include "Localisation/FormatArguments.hpp"
#include "Localisation/StringIds.h"
#include "Objects/InterfaceSkinObject.h"
#include "Objects/ObjectManager.h"
#include "Scenario.h"
#include "Ui/Widget.h"
#include "Ui/Widgets/ButtonWidget.h"
#include "Ui/Widgets/CaptionWidget.h"
#include "Ui/Widgets/PanelWidget.h"
#include "Ui/WindowManager.h"

namespace OpenLoco::Ui::Windows::LandscapeGenerationConfirm
{
    static constexpr Ui::Size32 kWindowSize = { 280, 92 };

    enum widx
    {
        panel = 0,
        caption = 1,
        close_button = 2,
        button_ok = 3,
        button_cancel = 4,
    };

    static constexpr auto widgets = makeWidgets(
        Widgets::Panel({ 0, 0 }, { 280, 92 }, WindowColour::primary),
        Widgets::Caption({ 1, 1 }, { 278, 13 }, Widgets::Caption::Style::boxed, WindowColour::primary),
        Widgets::Button({ 267, 2 }, { 11, 11 }, WindowColour::primary, StringIds::close_window_cross, StringIds::tooltip_close_window),
        Widgets::Button({ 20, 77 }, { 100, 12 }, WindowColour::primary, StringIds::label_ok),
        Widgets::Button({ 160, 77 }, { 100, 12 }, WindowColour::primary, StringIds::label_button_cancel)

    );

    // 0x004C18A5
    static void draw(Window& window, Gfx::DrawingContext& drawingCtx)
    {
        auto tr = Gfx::TextRenderer(drawingCtx);

        window.draw(drawingCtx);

        FormatArguments args{};
        if (window.var_846 == 0)
        {
            args.push(StringIds::prompt_confirm_generate_landscape);
        }
        else
        {
            args.push(StringIds::prompt_confirm_random_landscape);
        }

        auto origin = Ui::Point((window.width / 2), 41);
        tr.drawStringCentredWrapped(origin, window.width, Colour::black, StringIds::wcolour2_stringid, args);
    }

    // 0x004C18E4
    static void onMouseUp(Window& window, WidgetIndex_t widgetIndex, [[maybe_unused]] const WidgetId id)
    {
        switch (widgetIndex)
        {
            case widx::close_button:
            case widx::button_cancel:
                WindowManager::close(&window);
                break;

            case widx::button_ok:
                uint32_t status = window.var_846;
                WindowManager::close(&window);

                if (status == 0)
                {
                    Scenario::generateLandscape();
                }
                else
                {
                    Scenario::eraseLandscape();
                }
                break;
        }
    }

    static constexpr WindowEventList kEvents = {
        .onMouseUp = onMouseUp,
        .draw = draw,
    };

    static const WindowEventList& getEvents()
    {
        return kEvents;
    }

    // 0x004C180C
    Window* open(int32_t promptType)
    {
        auto window = WindowManager::bringToFront(WindowType::landscapeGenerationConfirm, 0);
        if (window == nullptr)
        {
            window = WindowManager::createWindowCentred(WindowType::landscapeGenerationConfirm, kWindowSize, WindowFlags::none, getEvents());
            window->setWidgets(widgets);
            window->initScrollWidgets();
            window->setColour(WindowColour::primary, AdvancedColour(Colour::mutedDarkRed).translucent());
            window->setColour(WindowColour::secondary, AdvancedColour(Colour::mutedDarkRed).translucent());
            window->flags |= WindowFlags::transparent;
        }

        window->var_846 = promptType;
        if (promptType == 0)
        {
            window->widgets[widx::caption].text = StringIds::title_generate_new_landscape;
        }
        else
        {
            window->widgets[widx::caption].text = StringIds::title_random_landscape_option;
        }

        return window;
    }
}
