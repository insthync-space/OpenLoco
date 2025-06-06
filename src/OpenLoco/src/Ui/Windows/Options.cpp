#include "Audio/Audio.h"
#include "Config.h"
#include "Date.h"
#include "Environment.h"
#include "Graphics/Colour.h"
#include "Graphics/Gfx.h"
#include "Graphics/ImageIds.h"
#include "Graphics/SoftwareDrawingEngine.h"
#include "Graphics/TextRenderer.h"
#include "Input.h"
#include "Jukebox.h"
#include "Localisation/FormatArguments.hpp"
#include "Localisation/LanguageFiles.h"
#include "Localisation/Languages.h"
#include "Localisation/StringIds.h"
#include "Objects/CompetitorObject.h"
#include "Objects/CurrencyObject.h"
#include "Objects/InterfaceSkinObject.h"
#include "Objects/ObjectIndex.h"
#include "Objects/ObjectManager.h"
#include "Scenario.h"
#include "ScenarioManager.h"
#include "ScenarioOptions.h"
#include "SceneManager.h"
#include "Ui.h"
#include "Ui/Dropdown.h"
#include "Ui/ToolManager.h"
#include "Ui/Widget.h"
#include "Ui/Widgets/ButtonWidget.h"
#include "Ui/Widgets/CaptionWidget.h"
#include "Ui/Widgets/CheckboxWidget.h"
#include "Ui/Widgets/DropdownWidget.h"
#include "Ui/Widgets/FrameWidget.h"
#include "Ui/Widgets/GroupBoxWidget.h"
#include "Ui/Widgets/ImageButtonWidget.h"
#include "Ui/Widgets/LabelWidget.h"
#include "Ui/Widgets/PanelWidget.h"
#include "Ui/Widgets/SliderWidget.h"
#include "Ui/Widgets/StepperWidget.h"
#include "Ui/Widgets/TabWidget.h"
#include "Ui/WindowManager.h"
#include <OpenLoco/Interop/Interop.hpp>
#include <cassert>

using namespace OpenLoco::Interop;

namespace OpenLoco::Ui::Windows::Options
{
    static void tabOnMouseUp(Window* w, WidgetIndex_t wi);
    static void sub_4C13BE(Window* w);
    static void setPreferredCurrencyNameBuffer();

    // Pointer to an array of SelectedObjectsFlags
    static loco_global<ObjectManager::SelectedObjectsFlags*, 0x011364A0> __11364A0;
    static loco_global<uint16_t, 0x0112C185> _112C185;

    // TODO: This shouldn't be required but its due to how the lifetime
    // of the string needs to exist beyond a prepare draw function and
    // into a draw function. Rework when custom formatter can store full
    // strings or widgets can store dynamically allocated strings.
    static std::string _chosenLanguage;

    struct AvailableCurrency
    {
        std::string name;
        ObjectHeader header;
        ObjectManager::ObjectIndexId index;
    };
    // We need to keep a copy due to lifetimes
    static sfl::small_vector<AvailableCurrency, 10> _availableCurrencies;

    static std::span<ObjectManager::SelectedObjectsFlags> getLoadedSelectedObjectFlags()
    {
        return std::span<ObjectManager::SelectedObjectsFlags>(*__11364A0, ObjectManager::getNumInstalledObjects());
    }

    static void populateAvailableCurrencies()
    {
        _availableCurrencies.clear();
        for (auto& object : ObjectManager::getAvailableObjects(ObjectType::currency))
        {
            _availableCurrencies.push_back(AvailableCurrency{ object.object._name, object.object._header, object.index });
        }
    }

    namespace Common
    {
        namespace Widx
        {
            enum
            {
                frame = 0,
                caption = 1,
                close_button = 2,
                panel = 3,
                tab_display,
                tab_sound,
                tab_music,
                tab_regional,
                tab_controls,
                tab_company,
                tab_miscellaneous,
            };
        }

        static_assert(Widx::tab_music == Widx::tab_display + kTabOffsetMusic);

        enum tab
        {
            display,
            sound,
            music,
            regional,
            controls,
            company,
            miscellaneous,
        };

        static void prepareDraw(Window& w)
        {
            static constexpr uint32_t music_tab_ids[] = {
                ImageIds::tab_music_0,
                ImageIds::tab_music_1,
                ImageIds::tab_music_2,
                ImageIds::tab_music_3,
                ImageIds::tab_music_4,
                ImageIds::tab_music_5,
                ImageIds::tab_music_6,
                ImageIds::tab_music_7,
                ImageIds::tab_music_8,
                ImageIds::tab_music_9,
                ImageIds::tab_music_10,
                ImageIds::tab_music_11,
                ImageIds::tab_music_12,
                ImageIds::tab_music_13,
                ImageIds::tab_music_14,
                ImageIds::tab_music_15,
            };

            // Music tab
            {
                auto imageId = music_tab_ids[0];
                if (w.currentTab == tab::music)
                {
                    imageId = music_tab_ids[(w.frameNo / 4) % 16];
                }
                w.widgets[Widx::tab_music].image = imageId;
            }

            static constexpr uint32_t globe_tab_ids[] = {
                ImageIds::tab_globe_0,
                ImageIds::tab_globe_1,
                ImageIds::tab_globe_2,
                ImageIds::tab_globe_3,
                ImageIds::tab_globe_4,
                ImageIds::tab_globe_5,
                ImageIds::tab_globe_6,
                ImageIds::tab_globe_7,
                ImageIds::tab_globe_8,
                ImageIds::tab_globe_9,
                ImageIds::tab_globe_10,
                ImageIds::tab_globe_11,
                ImageIds::tab_globe_12,
                ImageIds::tab_globe_13,
                ImageIds::tab_globe_14,
                ImageIds::tab_globe_15,
                ImageIds::tab_globe_16,
                ImageIds::tab_globe_17,
                ImageIds::tab_globe_18,
                ImageIds::tab_globe_19,
                ImageIds::tab_globe_20,
                ImageIds::tab_globe_21,
                ImageIds::tab_globe_22,
                ImageIds::tab_globe_23,
                ImageIds::tab_globe_24,
                ImageIds::tab_globe_25,
                ImageIds::tab_globe_26,
                ImageIds::tab_globe_27,
                ImageIds::tab_globe_28,
                ImageIds::tab_globe_29,
                ImageIds::tab_globe_30,
                ImageIds::tab_globe_31,
            };

            // Regional tab
            {
                auto imageId = ImageIds::tab_globe_0;
                if (w.currentTab == tab::regional)
                {
                    imageId = globe_tab_ids[(w.frameNo / 2) % 32];
                }
                w.widgets[Widx::tab_regional].image = imageId;
            }

            // Company tab
            {
                auto skin = ObjectManager::get<InterfaceSkinObject>();
                const uint32_t imageId = skin->img + InterfaceSkin::ImageIds::tab_company;
                w.widgets[Widx::tab_company].image = imageId;
            }
        }

        static void onClose([[maybe_unused]] Window& w)
        {
            ObjectManager::freeTemporaryObject();
            free(__11364A0);
        }

        static constexpr auto makeCommonWidgets(Ui::Size32 windowSize, StringId windowCaptionId)
        {
            return makeWidgets(
                Widgets::Frame({ 0, 0 }, windowSize, WindowColour::primary),
                Widgets::Caption({ 1, 1 }, { (uint16_t)(windowSize.width - 2), 13 }, Widgets::Caption::Style::whiteText, WindowColour::primary, windowCaptionId),
                Widgets::ImageButton({ (int16_t)(windowSize.width - 15), 2 }, { 13, 13 }, WindowColour::primary, ImageIds::close_button, StringIds::tooltip_close_window),
                Widgets::Panel({ 0, 41 }, { windowSize.width, 102 }, WindowColour::secondary),
                Widgets::Tab({ 3, 15 }, { 31, 27 }, WindowColour::secondary, ImageIds::tab_display, StringIds::tooltip_display_options),
                Widgets::Tab({ 34, 15 }, { 31, 27 }, WindowColour::secondary, ImageIds::tab_sound, StringIds::tooltip_sound_options),
                Widgets::Tab({ 65, 15 }, { 31, 27 }, WindowColour::secondary, ImageIds::tab_music_0, StringIds::tooltip_music_options),
                Widgets::Tab({ 96, 15 }, { 31, 27 }, WindowColour::secondary, ImageIds::tab_globe_0, StringIds::tooltip_regional_options),
                Widgets::Tab({ 127, 15 }, { 31, 27 }, WindowColour::secondary, ImageIds::tab_control, StringIds::tooltip_control_options),
                Widgets::Tab({ 158, 15 }, { 31, 27 }, WindowColour::secondary, ImageIds::tab, StringIds::tooltip_company_options),
                Widgets::Tab({ 189, 15 }, { 31, 27 }, WindowColour::secondary, ImageIds::tab_miscellaneous, StringIds::tooltip_miscellaneous_options));
        }

        static constexpr int tabWidgets = (1ULL << Widx::tab_display)
            | (1ULL << Widx::tab_sound)
            | (1ULL << Widx::tab_music)
            | (1ULL << Widx::tab_regional)
            | (1ULL << Widx::tab_controls)
            | (1ULL << Widx::tab_company)
            | (1ULL << Widx::tab_miscellaneous);

    }

    namespace Display
    {
        static constexpr Ui::Size32 kWindowSize = { 400, 266 };

        namespace Widx
        {
            enum
            {
                frame_hardware = Common::Widx::tab_miscellaneous + 1,
                screen_mode_label,
                screen_mode,
                screen_mode_btn,
                display_resolution_label,
                display_resolution,
                display_resolution_btn,
                display_scale_label,
                display_scale,
                display_scale_down_btn,
                display_scale_up_btn,
                uncap_fps,
                show_fps,
                frame_map_rendering,
                vehicles_min_scale_label,
                vehicles_min_scale,
                vehicles_min_scale_btn,
                station_names_min_scale_label,
                station_names_min_scale,
                station_names_min_scale_btn,
                construction_marker_label,
                construction_marker,
                construction_marker_btn,
                landscape_smoothing,
                gridlines_on_landscape,
                cash_popup_rendering,
            };
        }

        static constexpr auto _widgets = makeWidgets(
            Common::makeCommonWidgets(kWindowSize, StringIds::options_title_display),
            Widgets::GroupBox({ 4, 49 }, { 392, 97 }, WindowColour::secondary, StringIds::frame_hardware),

            Widgets::Label({ 10, 63 }, { 215, 12 }, WindowColour::secondary, ContentAlign::left, StringIds::options_screen_mode),
            Widgets::dropdownWidgets({ 235, 63 }, { 154, 12 }, WindowColour::secondary, StringIds::empty),

            Widgets::Label({ 10, 79 }, { 215, 12 }, WindowColour::secondary, ContentAlign::left, StringIds::display_resolution),
            Widgets::dropdownWidgets({ 235, 79 }, { 154, 12 }, WindowColour::secondary, StringIds::display_resolution_dropdown_format),

            Widgets::Label({ 10, 95 }, { 215, 12 }, WindowColour::secondary, ContentAlign::left, StringIds::window_scale_factor),
            Widgets::stepperWidgets({ 235, 95 }, { 154, 12 }, WindowColour::secondary, StringIds::scale_formatted),

            Widgets::Checkbox({ 10, 111 }, { 174, 12 }, WindowColour::secondary, StringIds::option_uncap_fps, StringIds::option_uncap_fps_tooltip),
            Widgets::Checkbox({ 10, 127 }, { 174, 12 }, WindowColour::secondary, StringIds::option_show_fps_counter, StringIds::option_show_fps_counter_tooltip),

            Widgets::GroupBox({ 4, 150 }, { 392, 112 }, WindowColour::secondary, StringIds::frame_map_rendering),

            Widgets::Label({ 10, 164 }, { 215, 12 }, WindowColour::secondary, ContentAlign::left, StringIds::vehicles_min_scale),
            Widgets::dropdownWidgets({ 235, 164 }, { 154, 12 }, WindowColour::secondary, StringIds::empty, StringIds::vehicles_min_scale_tip),

            Widgets::Label({ 10, 180 }, { 215, 12 }, WindowColour::secondary, ContentAlign::left, StringIds::station_names_min_scale),
            Widgets::dropdownWidgets({ 235, 180 }, { 154, 12 }, WindowColour::secondary, StringIds::empty, StringIds::station_names_min_scale_tip),

            Widgets::Label({ 10, 196 }, { 215, 12 }, WindowColour::secondary, ContentAlign::left, StringIds::construction_marker),
            Widgets::dropdownWidgets({ 235, 196 }, { 154, 12 }, WindowColour::secondary, StringIds::empty),

            Widgets::Checkbox({ 10, 211 }, { 346, 12 }, WindowColour::secondary, StringIds::landscape_smoothing, StringIds::landscape_smoothing_tip),
            Widgets::Checkbox({ 10, 227 }, { 346, 12 }, WindowColour::secondary, StringIds::gridlines_on_landscape, StringIds::gridlines_on_landscape_tip),
            Widgets::Checkbox({ 10, 243 }, { 346, 12 }, WindowColour::secondary, StringIds::cash_popup_rendering, StringIds::tooltip_cash_popup_rendering)

        );

        // 0x004BFB8C
        static void onMouseUp(Window& w, WidgetIndex_t wi, [[maybe_unused]] const WidgetId id)
        {
            switch (wi)
            {
                case Common::Widx::close_button:
                    WindowManager::close(&w);
                    return;

                case Common::Widx::tab_display:
                case Common::Widx::tab_sound:
                case Common::Widx::tab_music:
                case Common::Widx::tab_regional:
                case Common::Widx::tab_controls:
                case Common::Widx::tab_company:
                case Common::Widx::tab_miscellaneous:
                    Options::tabOnMouseUp(&w, wi);
                    return;

                case Widx::show_fps:
                {
                    auto& cfg = OpenLoco::Config::get();
                    cfg.showFPS ^= 1;
                    OpenLoco::Config::write();
                    Gfx::invalidateScreen();
                    return;
                }
                case Widx::uncap_fps:
                {
                    auto& cfg = OpenLoco::Config::get();
                    cfg.uncapFPS ^= 1;
                    OpenLoco::Config::write();
                    Gfx::invalidateScreen();
                    return;
                }

                case Widx::landscape_smoothing:
                {
                    auto& cfg = OpenLoco::Config::get();
                    // TODO: is there a better way to toggle a flag?
                    if (cfg.hasFlags(Config::Flags::landscapeSmoothing))
                    {
                        cfg.old.flags &= ~Config::Flags::landscapeSmoothing;
                    }
                    else
                    {
                        cfg.old.flags |= Config::Flags::landscapeSmoothing;
                    }
                    OpenLoco::Config::write();
                    Gfx::invalidateScreen();
                    return;
                }

                case Widx::gridlines_on_landscape:
                {
                    auto& cfg = OpenLoco::Config::get();
                    if (cfg.hasFlags(Config::Flags::gridlinesOnLandscape))
                    {
                        cfg.old.flags &= ~Config::Flags::gridlinesOnLandscape;
                    }
                    else
                    {
                        cfg.old.flags |= Config::Flags::gridlinesOnLandscape;
                    }
                    OpenLoco::Config::write();
                    Gfx::invalidateScreen();

                    auto main = WindowManager::getMainWindow();
                    if (main != nullptr)
                    {
                        main->viewports[0]->flags &= ~ViewportFlags::gridlines_on_landscape;

                        if (cfg.hasFlags(Config::Flags::gridlinesOnLandscape))
                        {
                            main->viewports[0]->flags |= ViewportFlags::gridlines_on_landscape;
                        }
                    }

                    return;
                }

                case Widx::cash_popup_rendering:
                {
                    auto& cfg = OpenLoco::Config::get();
                    cfg.cashPopupRendering = !cfg.cashPopupRendering;
                    Config::write();
                    w.invalidate();
                }
            }
        }

#pragma mark - Construction Marker (Widget 19)

        // 0x004BFE2E
        static void constructionMarkerMouseDown(Window* w, [[maybe_unused]] WidgetIndex_t wi)
        {
            Widget dropdown = w->widgets[Widx::construction_marker];
            Dropdown::show(w->x + dropdown.left, w->y + dropdown.top, dropdown.width() - 4, dropdown.height(), w->getColour(WindowColour::secondary), 2, 0x80);

            Dropdown::add(0, StringIds::dropdown_stringid, StringIds::white);
            Dropdown::add(1, StringIds::dropdown_stringid, StringIds::translucent);
            Dropdown::setItemSelected(Config::get().old.constructionMarker);
        }

        // 0x004BFE98
        static void constructionMarkerDropdown(int16_t ax)
        {
            if (ax == -1)
            {
                return;
            }

            if (ax == Config::get().old.constructionMarker)
            {
                return;
            }

            auto& cfg = OpenLoco::Config::get().old;
            cfg.constructionMarker = ax;
            OpenLoco::Config::write();
            Gfx::invalidateScreen();
        }

#pragma mark - Vehicle zoom (Widget 15)

        // 0x004BFEBE
        static void vehicleZoomMouseDown(Window* w, [[maybe_unused]] WidgetIndex_t wi)
        {
            Widget dropdown = w->widgets[Widx::vehicles_min_scale];
            Dropdown::show(w->x + dropdown.left, w->y + dropdown.top, dropdown.width() - 4, dropdown.height(), w->getColour(WindowColour::secondary), 4, 0x80);

            Dropdown::add(0, StringIds::dropdown_stringid, StringIds::full_scale);
            Dropdown::add(1, StringIds::dropdown_stringid, StringIds::half_scale);
            Dropdown::add(2, StringIds::dropdown_stringid, StringIds::quarter_scale);
            Dropdown::add(3, StringIds::dropdown_stringid, StringIds::eighth_scale);
            Dropdown::setItemSelected(Config::get().old.vehiclesMinScale);
        }

        // 0x004BFF4C
        static void vehicleZoomDropdown(int16_t ax)
        {
            if (ax == -1)
            {
                return;
            }

            if (ax == Config::get().old.vehiclesMinScale)
            {
                return;
            }

            auto& cfg = OpenLoco::Config::get().old;
            cfg.vehiclesMinScale = ax;
            OpenLoco::Config::write();
            Gfx::invalidateScreen();
        }

#pragma mark - Station names minimum scale (Widget 17)

        // 0x004BFF72
        static void stationNamesScaleMouseDown(Window* w, [[maybe_unused]] WidgetIndex_t wi)
        {
            Widget dropdown = w->widgets[Widx::station_names_min_scale];
            Dropdown::show(w->x + dropdown.left, w->y + dropdown.top, dropdown.width() - 4, dropdown.height(), w->getColour(WindowColour::secondary), 4, 0x80);

            Dropdown::add(0, StringIds::dropdown_stringid, StringIds::full_scale);
            Dropdown::add(1, StringIds::dropdown_stringid, StringIds::half_scale);
            Dropdown::add(2, StringIds::dropdown_stringid, StringIds::quarter_scale);
            Dropdown::add(3, StringIds::dropdown_stringid, StringIds::eighth_scale);
            Dropdown::setItemSelected(Config::get().old.stationNamesMinScale);
        }

        // 0x004C0000
        static void stationNamesScaleDropdown(int16_t ax)
        {
            if (ax == -1)
            {
                return;
            }

            if (ax == Config::get().old.stationNamesMinScale)
            {
                return;
            }

            auto& cfg = OpenLoco::Config::get().old;
            cfg.stationNamesMinScale = ax;
            OpenLoco::Config::write();
            Gfx::invalidateScreen();
        }

#if !(defined(__APPLE__) && defined(__MACH__))
        static void screenModeToggleEnabled(Window* w)
        {
            if (Config::get().display.mode == Config::ScreenMode::fullscreen)
            {
                w->disabledWidgets &= ~(1ULL << Widx::display_resolution) | (1ULL << Widx::display_resolution_btn);
                w->disabledWidgets &= ~((1ULL << Widx::display_resolution) | (1ULL << Widx::display_resolution_btn));
            }
            else
            {
                w->disabledWidgets |= ((1ULL << Widx::display_resolution) | (1ULL << Widx::display_resolution_btn));
                w->disabledWidgets |= (1ULL << Widx::display_resolution) | (1ULL << Widx::display_resolution_btn);
            }
        }
#endif

        static void screenModeMouseDown(Window* w, [[maybe_unused]] WidgetIndex_t wi)
        {
            Widget dropdown = w->widgets[Widx::screen_mode];
            Dropdown::show(w->x + dropdown.left, w->y + dropdown.top, dropdown.width() - 4, dropdown.height(), w->getColour(WindowColour::secondary), 3, 0x80);

            Dropdown::add(0, StringIds::dropdown_stringid, StringIds::options_mode_windowed);
            Dropdown::add(1, StringIds::dropdown_stringid, StringIds::options_mode_fullscreen);
            Dropdown::add(2, StringIds::dropdown_stringid, StringIds::options_mode_fullscreen_window);

            auto selection = static_cast<uint16_t>(Config::get().display.mode);
            Dropdown::setItemSelected(selection);
        }

        static void screenModeDropdown([[maybe_unused]] Window* w, int16_t selection)
        {
            if (selection == -1)
            {
                return;
            }

            auto new_mode = static_cast<Config::ScreenMode>(selection);
            if (new_mode == Config::get().display.mode)
            {
                return;
            }

#if !(defined(__APPLE__) && defined(__MACH__))
            Ui::setDisplayMode(new_mode);
#endif
        }

#pragma mark - Resolution dropdown (Widget 11)

        // 0x004C0026
        static void resolutionMouseDown(Window* w, [[maybe_unused]] WidgetIndex_t wi)
        {
            std::vector<Resolution> resolutions = getFullscreenResolutions();

            Widget dropdown = w->widgets[Widx::display_resolution];
            Dropdown::showText2(w->x + dropdown.left, w->y + dropdown.top, dropdown.width(), dropdown.height(), w->getColour(WindowColour::secondary), resolutions.size(), 0x80);

            auto& cfg = Config::get();
            for (size_t i = 0; i < resolutions.size(); i++)
            {
                Dropdown::add(i, StringIds::dropdown_stringid, { StringIds::display_resolution_dropdown_format, (uint16_t)resolutions[i].width, (uint16_t)resolutions[i].height });
                if (cfg.display.fullscreenResolution.width == resolutions[i].width && cfg.display.fullscreenResolution.height == resolutions[i].height)
                {
                    Dropdown::setItemSelected((int16_t)i);
                }
            }
        }

        // 0x004C00F4
        static void resolutionDropdown([[maybe_unused]] Window* w, int16_t index)
        {
            if (index == -1)
            {
                return;
            }
            std::vector<Resolution> resolutions = getFullscreenResolutions();
            Ui::setDisplayMode(Config::ScreenMode::fullscreen, { resolutions[index].width, resolutions[index].height });
        }

#pragma mark -

        static void displayScaleMouseDown([[maybe_unused]] Window* w, [[maybe_unused]] WidgetIndex_t wi, float adjust_by)
        {
            OpenLoco::Ui::adjustWindowScale(adjust_by);
        }

        // 0x004BFBB7
        static void onMouseDown(Window& w, WidgetIndex_t wi, [[maybe_unused]] const WidgetId id)
        {
            switch (wi)
            {
                case Widx::screen_mode_btn:
                    screenModeMouseDown(&w, wi);
                    break;
                case Widx::display_resolution_btn:
                    resolutionMouseDown(&w, wi);
                    break;
                case Widx::construction_marker_btn:
                    constructionMarkerMouseDown(&w, wi);
                    break;
                case Widx::vehicles_min_scale_btn:
                    vehicleZoomMouseDown(&w, wi);
                    break;
                case Widx::station_names_min_scale_btn:
                    stationNamesScaleMouseDown(&w, wi);
                    break;
                case Widx::display_scale_down_btn:
                    displayScaleMouseDown(&w, wi, -OpenLoco::Ui::ScaleFactor::step);
                    break;
                case Widx::display_scale_up_btn:
                    displayScaleMouseDown(&w, wi, OpenLoco::Ui::ScaleFactor::step);
                    break;
            }
        }

        // 0x004BFBE8
        static void onDropdown(Window& w, WidgetIndex_t wi, [[maybe_unused]] const WidgetId id, int16_t item_index)
        {
            switch (wi)
            {
                case Widx::screen_mode_btn:
                    screenModeDropdown(&w, item_index);
                    break;
                case Widx::display_resolution_btn:
                    resolutionDropdown(&w, item_index);
                    break;
                case Widx::construction_marker_btn:
                    constructionMarkerDropdown(item_index);
                    break;
                case Widx::vehicles_min_scale_btn:
                    vehicleZoomDropdown(item_index);
                    break;
                case Widx::station_names_min_scale_btn:
                    stationNamesScaleDropdown(item_index);
                    break;
            }
        }

        // 0x004C01F5
        static void onUpdate(Window& w)
        {
            w.frameNo += 1;
            w.callPrepareDraw();
            WindowManager::invalidateWidget(w.type, w.number, w.currentTab + 4);
        }

        // 0x004BFA04
        static void prepareDraw(Window& w)
        {
            assert(w.currentTab == Common::tab::display);

            w.activatedWidgets &= ~Common::tabWidgets;
            w.activatedWidgets |= 1ULL << (w.currentTab + 4);

            w.widgets[Common::Widx::frame].right = w.width - 1;
            w.widgets[Common::Widx::frame].bottom = w.height - 1;
            w.widgets[Common::Widx::panel].right = w.width - 1;
            w.widgets[Common::Widx::panel].bottom = w.height - 1;
            w.widgets[Common::Widx::caption].right = w.width - 2;
            w.widgets[Common::Widx::close_button].left = w.width - 15;
            w.widgets[Common::Widx::close_button].right = w.width - 15 + 12;

            StringId screenModeStringId = StringIds::empty;
            switch (Config::get().display.mode)
            {
                case Config::ScreenMode::window:
                    screenModeStringId = StringIds::options_mode_windowed;
                    break;
                case Config::ScreenMode::fullscreen:
                    screenModeStringId = StringIds::options_mode_fullscreen;
                    break;
                case Config::ScreenMode::fullscreenBorderless:
                    screenModeStringId = StringIds::options_mode_fullscreen_window;
                    break;
            }
            w.widgets[Widx::screen_mode].text = screenModeStringId;

            // Resolution.
            {
                auto args = FormatArguments(w.widgets[Widx::display_resolution].textArgs);
                auto& resolution = Config::get().display.fullscreenResolution;
                args.push<uint16_t>(resolution.width);
                args.push<uint16_t>(resolution.height);
            }

            // Scale.
            {
                auto args = FormatArguments(w.widgets[Widx::display_scale].textArgs);
                args.push<int32_t>(Config::get().scaleFactor * 100);
            }

            if (Config::get().old.constructionMarker)
            {
                w.widgets[Widx::construction_marker].text = StringIds::translucent;
            }
            else
            {
                w.widgets[Widx::construction_marker].text = StringIds::white;
            }

            static constexpr StringId kScaleStringIds[] = {
                StringIds::full_scale,
                StringIds::half_scale,
                StringIds::quarter_scale,
                StringIds::eighth_scale,
            };

            w.widgets[Widx::vehicles_min_scale].text = kScaleStringIds[Config::get().old.vehiclesMinScale];
            w.widgets[Widx::station_names_min_scale].text = kScaleStringIds[Config::get().old.stationNamesMinScale];

            w.activatedWidgets &= ~(1ULL << Widx::show_fps);
            if (Config::get().showFPS)
            {
                w.activatedWidgets |= (1ULL << Widx::show_fps);
            }

            w.activatedWidgets &= ~(1ULL << Widx::uncap_fps);
            if (Config::get().uncapFPS)
            {
                w.activatedWidgets |= (1ULL << Widx::uncap_fps);
            }

            w.activatedWidgets &= ~(1ULL << Widx::landscape_smoothing);
            if (!Config::get().hasFlags(Config::Flags::landscapeSmoothing))
            {
                w.activatedWidgets |= (1ULL << Widx::landscape_smoothing);
            }

            w.activatedWidgets &= ~(1ULL << Widx::gridlines_on_landscape);
            if (Config::get().hasFlags(Config::Flags::gridlinesOnLandscape))
            {
                w.activatedWidgets |= (1ULL << Widx::gridlines_on_landscape);
            }

            w.activatedWidgets &= ~(1ULL << Widx::cash_popup_rendering);
            if (Config::get().cashPopupRendering)
            {
                w.activatedWidgets |= (1ULL << Widx::cash_popup_rendering);
            }
            else
            {
                w.activatedWidgets &= ~(1ULL << Widx::cash_popup_rendering);
            }

            if (Config::get().scaleFactor <= OpenLoco::Ui::ScaleFactor::min)
            {
                w.disabledWidgets |= (1ULL << Widx::display_scale_down_btn);
            }
            else
            {
                w.disabledWidgets &= ~(1ULL << Widx::display_scale_down_btn);
            }

            if (Config::get().scaleFactor >= OpenLoco::Ui::ScaleFactor::max)
            {
                w.disabledWidgets |= (1ULL << Widx::display_scale_up_btn);
            }
            else
            {
                w.disabledWidgets &= ~(1ULL << Widx::display_scale_up_btn);
            }

#if !(defined(__APPLE__) && defined(__MACH__))
            screenModeToggleEnabled(&w);
#endif

            sub_4C13BE(&w);

            Common::prepareDraw(w);
        }

        // 0x004BFAF9
        static void draw(Window& w, Gfx::DrawingContext& drawingCtx)
        {
            // Draw widgets.
            w.draw(drawingCtx);
        }

        static void applyScreenModeRestrictions(Window* w)
        {
            if (Config::get().display.mode != Config::ScreenMode::fullscreen)
            {
                w->disabledWidgets = (1ULL << Display::Widx::display_resolution) | (1ULL << Display::Widx::display_resolution_btn);
            }

#if !(defined(__APPLE__) && defined(__MACH__))
            Display::screenModeToggleEnabled(w);
#else
            w->disabledWidgets |= (1ULL << Display::Widx::screen_mode)
                | (1ULL << Display::Widx::screen_mode_btn)
                | (1ULL << Display::Widx::display_resolution)
                | (1ULL << Display::Widx::display_resolution_btn);
#endif
        }

        static constexpr WindowEventList kEvents = {
            .onClose = Common::onClose,
            .onMouseUp = onMouseUp,
            .onMouseDown = onMouseDown,
            .onDropdown = onDropdown,
            .onUpdate = onUpdate,
            .prepareDraw = prepareDraw,
            .draw = draw,
        };

        static const WindowEventList& getEvents()
        {
            return kEvents;
        }
    }

    namespace Sound
    {
        static constexpr Ui::Size32 kWindowSize = { 366, 84 };

        namespace Widx
        {
            enum
            {
                audio_device = Common::Widx::tab_miscellaneous + 1,
                audio_device_btn,
                play_title_music,
            };
        }

        static constexpr auto _widgets = makeWidgets(
            Common::makeCommonWidgets(kWindowSize, StringIds::options_title_sound),
            Widgets::dropdownWidgets({ 10, 49 }, { 346, 12 }, WindowColour::secondary, StringIds::stringid),
            Widgets::Checkbox({ 10, 65 }, { 346, 12 }, WindowColour::secondary, StringIds::play_title_music)

        );

        static void audioDeviceMouseDown(Ui::Window* window);
        static void audioDeviceDropdown(Ui::Window* window, int16_t itemIndex);
        static void playTitleMusicOnMouseUp(Ui::Window* window);

        // 0x004C0217
        static void prepareDraw(Window& w)
        {
            assert(w.currentTab == Common::tab::sound);

            w.activatedWidgets &= ~Common::tabWidgets;
            w.activatedWidgets |= 1ULL << (w.currentTab + 4);

            w.widgets[Common::Widx::frame].right = w.width - 1;
            w.widgets[Common::Widx::frame].bottom = w.height - 1;
            w.widgets[Common::Widx::panel].right = w.width - 1;
            w.widgets[Common::Widx::panel].bottom = w.height - 1;
            w.widgets[Common::Widx::caption].right = w.width - 2;
            w.widgets[Common::Widx::close_button].left = w.width - 15;
            w.widgets[Common::Widx::close_button].right = w.width - 15 + 12;

            auto args = FormatArguments(w.widgets[Widx::audio_device].textArgs);

            auto audioDeviceName = Audio::getCurrentDeviceName();
            if (audioDeviceName != nullptr)
            {
                args.push(StringIds::stringptr);
                args.push(audioDeviceName);
            }
            else
            {
                args.push(StringIds::audio_device_none);
            }

            if (Config::get().audio.playTitleMusic)
            {
                w.activatedWidgets |= (1ULL << Widx::play_title_music);
            }
            else
            {
                w.activatedWidgets &= ~(1ULL << Widx::play_title_music);
            }

            sub_4C13BE(&w);

            Common::prepareDraw(w);
        }

        // 0x004C02F5
        static void draw(Window& w, Gfx::DrawingContext& drawingCtx)
        {
            // Draw widgets.
            w.draw(drawingCtx);
        }

        static void onMouseUp(Window& w, WidgetIndex_t wi, [[maybe_unused]] const WidgetId id)
        {
            switch (wi)
            {
                case Common::Widx::close_button:
                    WindowManager::close(&w);
                    return;

                case Common::Widx::tab_display:
                case Common::Widx::tab_sound:
                case Common::Widx::tab_music:
                case Common::Widx::tab_regional:
                case Common::Widx::tab_controls:
                case Common::Widx::tab_company:
                case Common::Widx::tab_miscellaneous:
                    Options::tabOnMouseUp(&w, wi);
                    return;

                case Widx::play_title_music:
                    playTitleMusicOnMouseUp(&w);
                    return;
            }
        }

        static void onMouseDown(Window& w, WidgetIndex_t wi, [[maybe_unused]] const WidgetId id)
        {
            switch (wi)
            {
                case Widx::audio_device_btn:
                    audioDeviceMouseDown(&w);
                    break;
            }
        }

        static void onDropdown(Ui::Window& window, WidgetIndex_t widgetIndex, [[maybe_unused]] const WidgetId id, int16_t itemIndex)
        {
            switch (widgetIndex)
            {
                case Widx::audio_device_btn:
                    audioDeviceDropdown(&window, itemIndex);
                    break;
            }
        }

#pragma mark - Widget 11

        // 0x004C043D
        static void audioDeviceMouseDown(Ui::Window* w)
        {
            const auto& devices = Audio::getDevices();
            if (devices.size() != 0)
            {
                Widget dropdown = w->widgets[Widx::audio_device];
                Dropdown::show(w->x + dropdown.left, w->y + dropdown.top, dropdown.width() - 4, dropdown.height(), w->getColour(WindowColour::secondary), devices.size(), 0x80);
                for (size_t i = 0; i < devices.size(); i++)
                {
                    auto name = devices[i].c_str();
                    Dropdown::add(i, StringIds::dropdown_stringid, { StringIds::stringptr, name });
                }

                auto currentDevice = Audio::getCurrentDevice();
                if (currentDevice != std::numeric_limits<size_t>().max())
                {
                    Dropdown::setItemSelected((int16_t)currentDevice);
                }
            }
        }

        // 0x004C04CA
        static void audioDeviceDropdown(Ui::Window* w, int16_t itemIndex)
        {
            if (itemIndex != -1)
            {
                Audio::setDevice(itemIndex);
                WindowManager::invalidateWidget(w->type, w->number, Widx::audio_device);
            }
        }

#pragma mark -

        static void playTitleMusicOnMouseUp(Window* w)
        {
            auto& cfg = Config::get();
            cfg.audio.playTitleMusic = !cfg.audio.playTitleMusic;
            Config::write();
            w->invalidate();

            if (!SceneManager::isTitleMode())
            {
                return;
            }

            if (cfg.audio.playTitleMusic)
            {
                Audio::playMusic(Environment::PathId::css5, Config::get().old.volume, true);
            }
            else
            {
                Audio::stopMusic();
            }
        }

        // 0x004C04E0
        static void onUpdate(Window& w)
        {
            w.frameNo += 1;
            w.callPrepareDraw();
            WindowManager::invalidateWidget(w.type, w.number, w.currentTab + 4);
        }

        static constexpr WindowEventList kEvents = {
            .onClose = Common::onClose,
            .onMouseUp = onMouseUp,
            .onMouseDown = onMouseDown,
            .onDropdown = onDropdown,
            .onUpdate = onUpdate,
            .prepareDraw = prepareDraw,
            .draw = draw,
        };

        static const WindowEventList& getEvents()
        {
            return kEvents;
        }
    }

    namespace Music
    {
        static constexpr Ui::Size32 kWindowSize = { 366, 129 };

        namespace Widx
        {
            enum
            {
                currently_playing_label = Common::Widx::tab_miscellaneous + 1,
                currently_playing,
                currently_playing_btn,
                music_controls_stop,
                music_controls_play,
                music_controls_next,
                volume_label,
                volume,
                music_playlist,
                music_playlist_btn,
                edit_selection
            };
        }

        static constexpr auto _widgets = makeWidgets(
            Common::makeCommonWidgets(kWindowSize, StringIds::options_title_music),
            Widgets::Label({ 10, 49 }, { 215, 12 }, WindowColour::secondary, ContentAlign::left, StringIds::currently_playing),
            Widgets::dropdownWidgets({ 160, 49 }, { 196, 12 }, WindowColour::secondary, StringIds::stringid),
            Widgets::ImageButton({ 10, 64 }, { 24, 24 }, WindowColour::secondary, ImageIds::music_controls_stop, StringIds::music_controls_stop_tip),
            Widgets::ImageButton({ 34, 64 }, { 24, 24 }, WindowColour::secondary, ImageIds::music_controls_play, StringIds::music_controls_play_tip),
            Widgets::ImageButton({ 58, 64 }, { 24, 24 }, WindowColour::secondary, ImageIds::music_controls_next, StringIds::music_controls_next_tip),
            Widgets::Label({ 183, 70 }, { 215, 12 }, WindowColour::secondary, ContentAlign::left, StringIds::volume),
            Widgets::Slider({ 256, 64 }, { 109, 24 }, WindowColour::secondary, Widget::kContentNull, StringIds::set_volume_tip),
            Widgets::dropdownWidgets({ 10, 93 }, { 346, 12 }, WindowColour::secondary, StringIds::stringid),
            Widgets::Button({ 183, 108 }, { 173, 12 }, WindowColour::secondary, StringIds::edit_music_selection, StringIds::edit_music_selection_tip)

        );

        static void volumeMouseDown(Window* w);
        static void stopMusic(Window* w);
        static void playMusic(Window* w);
        static void playNextSong(Window* w);
        static void musicPlaylistMouseDown(Window* w);
        static void musicPlaylistDropdown(Window* w, int16_t ax);
        static void currentlyPlayingMouseDown(Window* w);
        static void currentlyPlayingDropdown(Window* w, int16_t ax);

        static void prepareDraw(Window& w)
        {
            assert(w.currentTab == Common::tab::music);

            w.activatedWidgets &= ~Common::tabWidgets;
            w.activatedWidgets |= 1ULL << (w.currentTab + 4);

            w.widgets[Common::Widx::frame].right = w.width - 1;
            w.widgets[Common::Widx::frame].bottom = w.height - 1;
            w.widgets[Common::Widx::panel].right = w.width - 1;
            w.widgets[Common::Widx::panel].bottom = w.height - 1;
            w.widgets[Common::Widx::caption].right = w.width - 2;
            w.widgets[Common::Widx::close_button].left = w.width - 15;
            w.widgets[Common::Widx::close_button].right = w.width - 15 + 12;

            {
                StringId songName = Jukebox::getSelectedTrackTitleId();

                auto args = FormatArguments(w.widgets[Widx::currently_playing].textArgs);
                args.push(songName);
            }

            {
                static constexpr StringId playlist_string_ids[] = {
                    StringIds::play_only_music_from_current_era,
                    StringIds::play_all_music,
                    StringIds::play_custom_music_selection,
                };

                auto args = FormatArguments(w.widgets[Widx::music_playlist].textArgs);

                StringId currentSongStringId = playlist_string_ids[(uint8_t)Config::get().old.musicPlaylist];
                args.push(currentSongStringId);
            }

            w.activatedWidgets &= ~((1ULL << Widx::music_controls_stop) | (1ULL << Widx::music_controls_play));
            w.activatedWidgets |= (1ULL << Widx::music_controls_stop);
            if (Jukebox::isMusicPlaying())
            {
                w.activatedWidgets &= ~((1ULL << Widx::music_controls_stop) | (1ULL << Widx::music_controls_play));
                w.activatedWidgets |= (1ULL << Widx::music_controls_play);
            }

            w.disabledWidgets |= (1ULL << Widx::edit_selection);
            if (Config::get().old.musicPlaylist == Config::MusicPlaylistType::custom)
            {
                w.disabledWidgets &= ~(1ULL << Widx::edit_selection);
            }

            sub_4C13BE(&w);

            Common::prepareDraw(w);
        }

        // 0x004C05F9
        static void draw(Window& self, Gfx::DrawingContext& drawingCtx)
        {
            // Draw widgets.
            self.draw(drawingCtx);

            // TODO: Move this in Slider widget.
            drawingCtx.drawImage(self.widgets[Widx::volume].left, self.widgets[Widx::volume].top, Gfx::recolour(ImageIds::volume_slider_track, self.getColour(WindowColour::secondary).c()));

            int16_t x = 90 + (Config::get().old.volume / 32);
            drawingCtx.drawImage(self.widgets[Widx::volume].left + x, self.widgets[Widx::volume].top, Gfx::recolour(ImageIds::volume_slider_thumb, self.getColour(WindowColour::secondary).c()));
        }

        static void onMouseUp(Window& w, WidgetIndex_t wi, [[maybe_unused]] const WidgetId id)
        {
            switch (wi)
            {
                case Common::Widx::close_button:
                    WindowManager::close(&w);
                    return;

                case Common::Widx::tab_display:
                case Common::Widx::tab_sound:
                case Common::Widx::tab_music:
                case Common::Widx::tab_regional:
                case Common::Widx::tab_controls:
                case Common::Widx::tab_company:
                case Common::Widx::tab_miscellaneous:
                    Options::tabOnMouseUp(&w, wi);
                    return;

                case Widx::music_controls_stop:
                    stopMusic(&w);
                    return;

                case Widx::music_controls_play:
                    playMusic(&w);
                    return;

                case Widx::music_controls_next:
                    playNextSong(&w);
                    return;

                case Widx::edit_selection:
                    MusicSelection::open();
                    return;
            }
        }

        // 0x004C06F2
        static void onMouseDown(Window& w, WidgetIndex_t wi, [[maybe_unused]] const WidgetId id)
        {
            switch (wi)
            {
                case Widx::music_playlist_btn:
                    musicPlaylistMouseDown(&w);
                    break;
                case Widx::currently_playing_btn:
                    currentlyPlayingMouseDown(&w);
                    break;
                case Widx::volume:
                    volumeMouseDown(&w);
                    break;
            }
        }

        // 0x004C070D
        static void onDropdown(Ui::Window& window, WidgetIndex_t widgetIndex, [[maybe_unused]] const WidgetId id, int16_t itemIndex)
        {
            switch (widgetIndex)
            {
                case Widx::music_playlist_btn:
                    musicPlaylistDropdown(&window, itemIndex);
                    break;
                case Widx::currently_playing_btn:
                    currentlyPlayingDropdown(&window, itemIndex);
                    break;
            }
        }

        // 0x004C072A
        static void volumeMouseDown(Window* w)
        {
            Input::setClickRepeatTicks(31);

            auto mousePos = Input::getScrollLastLocation();
            int x = mousePos.x - w->x - w->widgets[Widx::volume].left - 10;
            x = std::clamp(x, 0, 80);

            Audio::setBgmVolume((x * 32) - 2560);

            w->invalidate();
        }

        // 0x004C0778
        static void stopMusic(Window* w)
        {
            if (Jukebox::disableMusic())
            {
                w->invalidate();
            }
        }

        // 0x004C07A4
        static void playMusic(Window* w)
        {
            if (Jukebox::enableMusic())
            {
                w->invalidate();
            }
        }

        // 0x004C07C4
        static void playNextSong(Window* w)
        {
            if (Jukebox::skipCurrentTrack())
            {
                w->invalidate();
            }
        }

#pragma mark - Widget 17

        // 0x004C07E4
        static void musicPlaylistMouseDown(Window* w)
        {
            Widget dropdown = w->widgets[Widx::music_playlist];
            Dropdown::show(w->x + dropdown.left, w->y + dropdown.top, dropdown.width() - 4, dropdown.height(), w->getColour(WindowColour::secondary), 3, 0x80);

            Dropdown::add(0, StringIds::dropdown_stringid, StringIds::play_only_music_from_current_era);
            Dropdown::add(1, StringIds::dropdown_stringid, StringIds::play_all_music);
            Dropdown::add(2, StringIds::dropdown_stringid, StringIds::play_custom_music_selection);

            Dropdown::setItemSelected((uint8_t)Config::get().old.musicPlaylist);
        }

        // 0x004C084A
        static void musicPlaylistDropdown(Window* w, int16_t ax)
        {
            if (ax == -1)
            {
                return;
            }

            auto& cfg = OpenLoco::Config::get().old;
            cfg.musicPlaylist = (Config::MusicPlaylistType)ax;
            Config::write();

            w->invalidate();

            Audio::revalidateCurrentTrack();

            WindowManager::close(WindowType::musicSelection);
        }

#pragma mark - Widget 11

        // 0x004C0875
        static void currentlyPlayingMouseDown(Window* w)
        {
            auto tracks = Jukebox::makeSelectedPlaylist();

            Widget dropdown = w->widgets[Widx::currently_playing];
            Dropdown::show(w->x + dropdown.left, w->y + dropdown.top, dropdown.width() - 4, dropdown.height(), w->getColour(WindowColour::secondary), tracks.size(), 0x80);

            int index = -1;
            for (auto track : tracks)
            {
                index++;
                Dropdown::add(index, StringIds::dropdown_stringid, Jukebox::getMusicInfo(track).titleId);
                if (track == Jukebox::getCurrentTrack())
                {
                    Dropdown::setItemSelected(index);
                }
            }
        }

        // 0x004C09F8
        static void currentlyPlayingDropdown(Window* w, int16_t ax)
        {
            if (ax == -1)
            {
                return;
            }

            auto track = Jukebox::makeSelectedPlaylist().at(ax);
            if (Jukebox::requestTrack(track))
            {
                w->invalidate();
            }
        }

        // 0x004C0A37
        static void onUpdate(Window& w)
        {
            w.frameNo += 1;
            w.callPrepareDraw();
            WindowManager::invalidateWidget(w.type, w.number, w.currentTab + 4);
        }

        static constexpr WindowEventList kEvents = {
            .onClose = Common::onClose,
            .onMouseUp = onMouseUp,
            .onMouseDown = onMouseDown,
            .onDropdown = onDropdown,
            .onUpdate = onUpdate,
            .prepareDraw = prepareDraw,
            .draw = draw,
        };

        static const WindowEventList& getEvents()
        {
            return kEvents;
        }
    }

    namespace Regional
    {
        static constexpr Ui::Size32 kWindowSize = { 366, 167 };

        namespace Widx
        {
            enum
            {
                language_label = Common::Widx::tab_miscellaneous + 1,
                language,
                language_btn,
                distance_label,
                distance_speed,
                distance_speed_btn,
                heights_label,
                heights,
                heights_btn,
                currency_label,
                currency,
                currency_btn,
                preferred_currency_label,
                preferred_currency,
                preferred_currency_btn,
                preferred_currency_for_new_games,
                preferred_currency_always
            };
        }

        static constexpr auto _widgets = makeWidgets(
            Common::makeCommonWidgets(kWindowSize, StringIds::options_title_regional),

            Widgets::Label({ 10, 49 }, { 173, 12 }, WindowColour::secondary, ContentAlign::left, StringIds::options_language),
            Widgets::dropdownWidgets({ 183, 49 }, { 173, 12 }, WindowColour::secondary, StringIds::stringptr),

            Widgets::Label({ 10, 69 }, { 173, 12 }, WindowColour::secondary, ContentAlign::left, StringIds::distance_and_speed),
            Widgets::dropdownWidgets({ 183, 69 }, { 173, 12 }, WindowColour::secondary, StringIds::stringid),

            Widgets::Label({ 10, 84 }, { 173, 12 }, WindowColour::secondary, ContentAlign::left, StringIds::heights),
            Widgets::dropdownWidgets({ 183, 84 }, { 173, 12 }, WindowColour::secondary, StringIds::stringid),

            Widgets::Label({ 10, 104 }, { 173, 12 }, WindowColour::secondary, ContentAlign::left, StringIds::current_game_currency),
            Widgets::dropdownWidgets({ 183, 104 }, { 173, 12 }, WindowColour::secondary, StringIds::stringid, StringIds::current_game_currency_tip),

            Widgets::Label({ 10, 119 }, { 173, 12 }, WindowColour::secondary, ContentAlign::left, StringIds::new_game_currency),
            Widgets::dropdownWidgets({ 183, 119 }, { 173, 12 }, WindowColour::secondary, StringIds::preferred_currency_buffer, StringIds::new_game_currency_tip),

            Widgets::Checkbox({ 10, 134 }, { 346, 12 }, WindowColour::secondary, StringIds::use_preferred_currency_new_game, StringIds::use_preferred_currency_new_game_tip),
            Widgets::Checkbox({ 10, 148 }, { 346, 12 }, WindowColour::secondary, StringIds::use_preferred_currency_always, StringIds::use_preferred_currency_always_tip)

        );

        static void languageMouseDown(Window* w);
        static void languageDropdown(Window* w, int16_t ax);
        static void currencyMouseDown(Window* w);
        static void currencyDropdown(Window* w, int16_t ax);
        static void preferredCurrencyMouseDown(Window* w);
        static void preferredCurrencyDropdown(Window* w, int16_t ax);
        static void preferredCurrencyNewGameMouseUp(Window* w);
        static void preferredCurrencyAlwaysMouseUp(Window* w);
        static void distanceSpeedMouseDown(Window* w);
        static void distanceSpeedDropdown(Window* w, int16_t ax);
        static void heightsLabelsMouseDown(Window* w);
        static void heightsLabelsDropdown(Window* w, int16_t ax);

        // 0x004C0A59
        static void prepareDraw(Window& w)
        {
            assert(w.currentTab == Common::tab::regional);

            w.activatedWidgets &= ~Common::tabWidgets;
            w.activatedWidgets |= 1ULL << (w.currentTab + 4);

            w.widgets[Common::Widx::frame].right = w.width - 1;
            w.widgets[Common::Widx::frame].bottom = w.height - 1;
            w.widgets[Common::Widx::panel].right = w.width - 1;
            w.widgets[Common::Widx::panel].bottom = w.height - 1;
            w.widgets[Common::Widx::caption].right = w.width - 2;
            w.widgets[Common::Widx::close_button].left = w.width - 15;
            w.widgets[Common::Widx::close_button].right = w.width - 15 + 12;

            {
                auto args = FormatArguments(w.widgets[Widx::language].textArgs);

                auto& language = Localisation::getDescriptorForLanguage(Config::get().language);
                _chosenLanguage = language.nativeName;
                args.push(_chosenLanguage.c_str());
            }

            {
                auto args = FormatArguments(w.widgets[Widx::distance_speed].textArgs);

                StringId current_measurement_format = StringIds::imperial;
                if (OpenLoco::Config::get().old.measurementFormat == Config::MeasurementFormat::metric)
                {
                    current_measurement_format = StringIds::metric;
                }

                args.push(current_measurement_format);
            }

            {
                auto args = FormatArguments(w.widgets[Widx::currency].textArgs);
                args.push(ObjectManager::get<CurrencyObject>()->name);
            }

            {
                auto args = FormatArguments(w.widgets[Widx::heights].textArgs);

                StringId current_height_units = StringIds::height_units;
                if (!OpenLoco::Config::get().hasFlags(Config::Flags::showHeightAsUnits))
                {
                    current_height_units = StringIds::height_real_values;
                }

                args.push(current_height_units);
            }

            if (Config::get().usePreferredCurrencyForNewGames)
            {
                w.activatedWidgets |= (1ULL << Widx::preferred_currency_for_new_games);
            }
            else
            {
                w.activatedWidgets &= ~(1ULL << Widx::preferred_currency_for_new_games);
            }

            if (Config::get().usePreferredCurrencyAlways)
            {
                w.activatedWidgets |= (1ULL << Widx::preferred_currency_always);
            }
            else
            {
                w.activatedWidgets &= ~(1ULL << Widx::preferred_currency_always);
            }

            if (Config::get().usePreferredCurrencyAlways)
            {
                w.disabledWidgets |= (1ULL << Widx::currency);
                w.disabledWidgets |= (1ULL << Widx::currency_btn);
            }
            else
            {
                w.disabledWidgets &= ~(1ULL << Widx::currency);
                w.disabledWidgets &= ~(1ULL << Widx::currency_btn);
            }

            sub_4C13BE(&w);

            Common::prepareDraw(w);
        }

        // 0x004C0B5B
        static void draw(Window& w, Gfx::DrawingContext& drawingCtx)
        {
            // Draw widgets.
            w.draw(drawingCtx);
        }

        static void onMouseUp(Window& w, WidgetIndex_t wi, [[maybe_unused]] const WidgetId id)
        {
            switch (wi)
            {
                case Common::Widx::close_button:
                    WindowManager::close(&w);
                    return;

                case Common::Widx::tab_display:
                case Common::Widx::tab_sound:
                case Common::Widx::tab_music:
                case Common::Widx::tab_regional:
                case Common::Widx::tab_controls:
                case Common::Widx::tab_company:
                case Common::Widx::tab_miscellaneous:
                    Options::tabOnMouseUp(&w, wi);
                    return;

                case Widx::preferred_currency_for_new_games:
                    preferredCurrencyNewGameMouseUp(&w);
                    return;

                case Widx::preferred_currency_always:
                    preferredCurrencyAlwaysMouseUp(&w);
                    return;
            }
        }

        // 0x004BFBB7
        static void onMouseDown(Window& w, WidgetIndex_t wi, [[maybe_unused]] const WidgetId id)
        {
            switch (wi)
            {
                case Widx::language_btn:
                    languageMouseDown(&w);
                    break;
                case Widx::heights_btn:
                    heightsLabelsMouseDown(&w);
                    break;
                case Widx::distance_speed_btn:
                    distanceSpeedMouseDown(&w);
                    break;
                case Widx::currency_btn:
                    currencyMouseDown(&w);
                    break;
                case Widx::preferred_currency_btn:
                    preferredCurrencyMouseDown(&w);
                    break;
            }
        }

        // 0x004C0C4A
        static void onDropdown(Ui::Window& window, WidgetIndex_t widgetIndex, [[maybe_unused]] const WidgetId id, int16_t itemIndex)
        {
            switch (widgetIndex)
            {
                case Widx::language_btn:
                    languageDropdown(&window, itemIndex);
                    break;

                case Widx::heights_btn:
                    heightsLabelsDropdown(&window, itemIndex);
                    break;

                case Widx::distance_speed_btn:
                    distanceSpeedDropdown(&window, itemIndex);
                    break;

                case Widx::currency_btn:
                    currencyDropdown(&window, itemIndex);
                    break;

                case Widx::preferred_currency_btn:
                    preferredCurrencyDropdown(&window, itemIndex);
                    break;
            }
        }

        static loco_global<std::byte*, 0x0050D13C> _installedObjectList;

        static void languageMouseDown(Window* w)
        {
            const auto lds = Localisation::getLanguageDescriptors();
            uint8_t numLanguages = static_cast<uint8_t>(lds.size());

            Widget dropdown = w->widgets[Widx::language];
            Dropdown::show(w->x + dropdown.left, w->y + dropdown.top, dropdown.width() - 4, dropdown.height(), w->getColour(WindowColour::secondary), numLanguages - 1, 0x80);

            std::string& current_language = Config::get().language;

            for (uint8_t index = 1; index < numLanguages; index++)
            {
                auto& ld = lds[index];
                Dropdown::add(index - 1, StringIds::dropdown_stringptr, (char*)ld.nativeName.c_str());

                if (ld.locale == current_language)
                {
                    Dropdown::setItemSelected(index - 1);
                }
            }
        }

        static void languageDropdown(Window* w, int16_t ax)
        {
            if (ax == -1)
            {
                w->invalidate();
                return;
            }

            const auto lds = Localisation::getLanguageDescriptors();
            const auto& ld = lds[ax + 1];
            Config::get().language = ld.locale;
            Config::write();
            Localisation::loadLanguageFile();
            // Reloading the objects will force objects to load the new language
            ObjectManager::reloadAll();
            Gfx::invalidateScreen();

            // Rebuild the scenario index to use the new language.
            ScenarioManager::loadIndex(true);
        }

        // 0x004C0C73
        static void currencyMouseDown(Window* w)
        {
            const auto selectedObjectFlags = getLoadedSelectedObjectFlags();

            Widget dropdown = w->widgets[Widx::currency];
            Dropdown::show(w->x + dropdown.left, w->y + dropdown.top, dropdown.width() - 4, dropdown.height(), w->getColour(WindowColour::secondary), _112C185, 0x80);
            int index = -1;
            for (auto& object : _availableCurrencies)
            {
                index++;
                Dropdown::add(index, StringIds::dropdown_stringptr, object.name.c_str());

                if ((selectedObjectFlags[object.index] & ObjectManager::SelectedObjectsFlags::selected) != ObjectManager::SelectedObjectsFlags::none)
                {
                    Dropdown::setItemSelected(index);
                }
            }
        }

        // 0x004C0D33
        static void currencyDropdown(Window* w, int16_t ax)
        {
            if (ax == -1)
            {
                w->invalidate();
                return;
            }

            const auto selectedObjectFlags = getLoadedSelectedObjectFlags();

            int index = -1;
            for (const auto& object : _availableCurrencies)
            {
                index++;
                if (index == ax)
                {
                    auto ebp = ObjectManager::getActiveObject(ObjectType::currency, selectedObjectFlags);

                    if (ebp.index != ObjectManager::kNullObjectIndex)
                    {
                        ObjectManager::unload(ebp.object._header);
                    }

                    ObjectManager::load(object.header);
                    ObjectManager::reloadAll();
                    Gfx::loadCurrency();
                    ObjectManager::markOnlyLoadedObjects(selectedObjectFlags);

                    break;
                }
            }

            w->invalidate();
        }

        // 0x004C0DCF
        static void preferredCurrencyMouseDown(Window* w)
        {
            Widget dropdown = w->widgets[Widx::preferred_currency];
            Dropdown::show(w->x + dropdown.left, w->y + dropdown.top, dropdown.width() - 4, dropdown.height(), w->getColour(WindowColour::secondary), _112C185, 0x80);

            int index = -1;
            for (auto& object : _availableCurrencies)
            {
                index++;
                Dropdown::add(index, StringIds::dropdown_stringptr, object.name.c_str());

                if (OpenLoco::Config::get().preferredCurrency == object.header)
                {
                    Dropdown::setItemSelected(index);
                }
            }
        }

        // 0x004C0E82
        static void preferredCurrencyDropdown(Window* w, int16_t ax)
        {
            if (ax == -1)
            {
                w->invalidate();
                return;
            }

            int index = -1;
            for (const auto& object : _availableCurrencies)
            {
                index++;

                if (index == ax)
                {
                    auto& cfg = OpenLoco::Config::get();
                    cfg.preferredCurrency = object.header;

                    setPreferredCurrencyNameBuffer();
                    Config::write();
                    Scenario::loadPreferredCurrencyAlways();
                    ObjectManager::markOnlyLoadedObjects(getLoadedSelectedObjectFlags());

                    break;
                }
            }

            w->invalidate();
        }

        // 0x004C0F14
        static void preferredCurrencyNewGameMouseUp(Window* w)
        {
            auto& cfg = OpenLoco::Config::get();
            cfg.usePreferredCurrencyForNewGames ^= true;
            Config::write();

            w->invalidate();
        }

        // 0x004C0F27
        static void preferredCurrencyAlwaysMouseUp(Window* w)
        {
            auto& cfg = OpenLoco::Config::get();
            cfg.usePreferredCurrencyAlways ^= true;
            Config::write();

            Scenario::loadPreferredCurrencyAlways();
            ObjectManager::markOnlyLoadedObjects(getLoadedSelectedObjectFlags());

            w->invalidate();
        }

        // 0x004C0F49
        static void distanceSpeedMouseDown(Window* w)
        {
            Widget dropdown = w->widgets[Widx::distance_speed];
            Dropdown::show(w->x + dropdown.left, w->y + dropdown.top, dropdown.width() - 4, dropdown.height(), w->getColour(WindowColour::secondary), 2, 0x80);

            Dropdown::add(0, StringIds::dropdown_stringid, StringIds::imperial);
            Dropdown::add(1, StringIds::dropdown_stringid, StringIds::metric);
            Dropdown::setItemSelected(static_cast<uint8_t>(Config::get().old.measurementFormat));
        }

        // 0x004C0FB3
        static void distanceSpeedDropdown([[maybe_unused]] Window* w, int16_t ax)
        {
            if (ax == -1)
            {
                return;
            }

            auto& cfg = Config::get();
            cfg.old.measurementFormat = Config::MeasurementFormat(ax);

            // 0x004C0FC2
            cfg.old.heightMarkerOffset = 0;
            if (!cfg.hasFlags(Config::Flags::showHeightAsUnits))
            {
                cfg.old.heightMarkerOffset = cfg.old.measurementFormat == Config::MeasurementFormat::imperial ? 0x100 : 0x200;
            }

            Config::write();
            Gfx::invalidateScreen();
        }

        // 0x004C0FFA
        static void heightsLabelsMouseDown(Window* w)
        {
            Widget dropdown = w->widgets[Widx::heights];
            Dropdown::show(w->x + dropdown.left, w->y + dropdown.top, dropdown.width() - 4, dropdown.height(), w->getColour(WindowColour::secondary), 2, 0x80);

            Dropdown::add(0, StringIds::dropdown_stringid, StringIds::height_units);
            Dropdown::add(1, StringIds::dropdown_stringid, StringIds::height_real_values);

            int selectedItem = 0;
            if (!Config::get().hasFlags(Config::Flags::showHeightAsUnits))
            {
                selectedItem = 1;
            }
            Dropdown::setItemSelected(selectedItem);
        }

        // 0x004C106C
        static void heightsLabelsDropdown([[maybe_unused]] Window* w, int16_t ax)
        {
            if (ax == -1)
            {
                return;
            }

            auto& cfg = Config::get().old;
            cfg.flags &= ~Config::Flags::showHeightAsUnits;

            if (ax == 0)
            {
                cfg.flags |= Config::Flags::showHeightAsUnits;
            }

            // 0x004C0FC2
            cfg.heightMarkerOffset = 0;
            if ((cfg.flags & Config::Flags::showHeightAsUnits) == Config::Flags::none)
            {
                cfg.heightMarkerOffset = cfg.measurementFormat == Config::MeasurementFormat::imperial ? 0x100 : 0x200;
            }

            Config::write();
            Gfx::invalidateScreen();
        }

        // 0x004C1195
        static void onUpdate(Window& w)
        {
            w.frameNo += 1;
            w.callPrepareDraw();
            WindowManager::invalidateWidget(w.type, w.number, w.currentTab + 4);
        }

        static constexpr WindowEventList kEvents = {
            .onClose = Common::onClose,
            .onMouseUp = onMouseUp,
            .onMouseDown = onMouseDown,
            .onDropdown = onDropdown,
            .onUpdate = onUpdate,
            .prepareDraw = prepareDraw,
            .draw = draw,
        };

        static const WindowEventList& getEvents()
        {
            return kEvents;
        }
    }

    namespace Controls
    {
        namespace Widx
        {
            enum
            {
                edge_scrolling = Common::Widx::tab_miscellaneous + 1,
                zoom_to_cursor,
                invertRightMouseViewPan,
                customize_keys
            };
        }

        static constexpr Ui::Size32 kWindowSize = { 366, 114 };

        static constexpr auto _widgets = makeWidgets(
            Common::makeCommonWidgets(kWindowSize, StringIds::options_title_controls),
            Widgets::Checkbox({ 10, 49 }, { 346, 12 }, WindowColour::secondary, StringIds::scroll_screen_edge, StringIds::scroll_screen_edge_tip),
            Widgets::Checkbox({ 10, 64 }, { 346, 12 }, WindowColour::secondary, StringIds::zoom_to_cursor, StringIds::zoom_to_cursor_tip),
            Widgets::Checkbox({ 10, 79 }, { 346, 12 }, WindowColour::secondary, StringIds::invert_right_mouse_dragging, StringIds::tooltip_invert_right_mouse_dragging),
            Widgets::Button({ 26, 94 }, { 160, 12 }, WindowColour::secondary, StringIds::customise_keys, StringIds::customise_keys_tip)

        );

        static void edgeScrollingMouseUp(Window* w);
        static void zoomToCursorMouseUp(Window* w);
        static void invertRightMouseViewPan(Window* w);
        static void openKeyboardShortcuts();

        static void prepareDraw(Window& w)
        {
            assert(w.currentTab == Common::tab::controls);

            w.activatedWidgets &= ~Common::tabWidgets;
            w.activatedWidgets |= 1ULL << (w.currentTab + 4);

            w.widgets[Common::Widx::frame].right = w.width - 1;
            w.widgets[Common::Widx::frame].bottom = w.height - 1;
            w.widgets[Common::Widx::panel].right = w.width - 1;
            w.widgets[Common::Widx::panel].bottom = w.height - 1;
            w.widgets[Common::Widx::caption].right = w.width - 2;
            w.widgets[Common::Widx::close_button].left = w.width - 15;
            w.widgets[Common::Widx::close_button].right = w.width - 15 + 12;

            w.activatedWidgets &= ~(1ULL << Widx::edge_scrolling | 1ULL << Widx::zoom_to_cursor | 1ULL << Widx::invertRightMouseViewPan);
            if (Config::get().edgeScrolling)
            {
                w.activatedWidgets |= (1ULL << Widx::edge_scrolling);
            }
            if (Config::get().zoomToCursor)
            {
                w.activatedWidgets |= (1ULL << Widx::zoom_to_cursor);
            }
            if (Config::get().invertRightMouseViewPan)
            {
                w.activatedWidgets |= (1ULL << Widx::invertRightMouseViewPan);
            }

            sub_4C13BE(&w);

            Common::prepareDraw(w);
        }

        // 0x004C113F
        static void draw(Window& w, Gfx::DrawingContext& drawingCtx)
        {
            w.draw(drawingCtx);
        }

        // 0x004C114A
        static void onMouseUp(Window& w, WidgetIndex_t wi, [[maybe_unused]] const WidgetId id)
        {
            switch (wi)
            {
                case Common::Widx::close_button:
                    WindowManager::close(&w);
                    return;

                case Common::Widx::tab_display:
                case Common::Widx::tab_sound:
                case Common::Widx::tab_music:
                case Common::Widx::tab_regional:
                case Common::Widx::tab_controls:
                case Common::Widx::tab_company:
                case Common::Widx::tab_miscellaneous:
                    Options::tabOnMouseUp(&w, wi);
                    return;

                case Widx::customize_keys:
                    openKeyboardShortcuts();
                    break;

                case Widx::edge_scrolling:
                    edgeScrollingMouseUp(&w);
                    break;

                case Widx::zoom_to_cursor:
                    zoomToCursorMouseUp(&w);
                    break;

                case Widx::invertRightMouseViewPan:
                    invertRightMouseViewPan(&w);
                    break;
            }
        }

        // 0x004C117A
        static void edgeScrollingMouseUp(Window* w)
        {
            auto& cfg = OpenLoco::Config::get();
            cfg.edgeScrolling = !cfg.edgeScrolling;
            Config::write();

            w->invalidate();
        }

        static void zoomToCursorMouseUp(Window* w)
        {
            auto& cfg = OpenLoco::Config::get();
            cfg.zoomToCursor = !cfg.zoomToCursor;
            Config::write();

            w->invalidate();
        }

        static void invertRightMouseViewPan(Window* w)
        {
            auto& cfg = OpenLoco::Config::get();
            cfg.invertRightMouseViewPan = !cfg.invertRightMouseViewPan;
            Config::write();

            w->invalidate();
        }

        // 0x004C118D
        static void openKeyboardShortcuts()
        {
            KeyboardShortcuts::open();
        }

        // 0x004C1195
        static void onUpdate(Window& w)
        {
            w.frameNo += 1;
            w.callPrepareDraw();
            WindowManager::invalidateWidget(w.type, w.number, w.currentTab + 4);
        }

        static constexpr WindowEventList kEvents = {
            .onClose = Common::onClose,
            .onMouseUp = onMouseUp,
            .onUpdate = onUpdate,
            .prepareDraw = prepareDraw,
            .draw = draw,
        };

        static const WindowEventList& getEvents()
        {
            return kEvents;
        }
    }

    namespace Company
    {
        static constexpr Ui::Size32 kWindowSize = { 420, 134 };

        namespace Widx
        {
            enum
            {
                groupPreferredOwner = Common::Widx::tab_miscellaneous + 1,

                usePreferredOwnerFace,
                changeOwnerFaceBtn,
                labelOwnerFace,

                usePreferredOwnerName,
                changeOwnerNameBtn,
                labelPreferredOwnerName,

                ownerFacePreview,
            };
        }

        static constexpr auto _widgets = makeWidgets(
            Common::makeCommonWidgets(kWindowSize, StringIds::options_title_company),

            // Preferred owner group
            Widgets::GroupBox({ 4, 50 }, { 412, 80 }, WindowColour::secondary, StringIds::preferred_owner_name),

            // Preferred owner face
            Widgets::Checkbox({ 10, 64 }, { 400, 12 }, WindowColour::secondary, StringIds::usePreferredCompanyFace, StringIds::usePreferredCompanyFaceTip),
            Widgets::Button({ 265, 79 }, { 75, 12 }, WindowColour::secondary, StringIds::change),
            Widgets::Label({ 24, 79 }, { 240, 12 }, WindowColour::secondary, ContentAlign::left, StringIds::currentPreferredFace),

            // Preferred owner name
            Widgets::Checkbox({ 10, 97 }, { 400, 12 }, WindowColour::secondary, StringIds::use_preferred_owner_name, StringIds::use_preferred_owner_name_tip),
            Widgets::Button({ 265, 112 }, { 75, 12 }, WindowColour::secondary, StringIds::change),
            Widgets::Label({ 24, 112 }, { 240, 12 }, WindowColour::secondary, ContentAlign::left, StringIds::wcolour2_preferred_owner_name),

            // Preferred owner preview
            Widgets::ImageButton({ 345, 59 }, { 66, 66 }, WindowColour::secondary, Widget::kContentNull)

        );

        static void loadPreferredFace(Window& self)
        {
            if (WindowManager::getCurrentModalType() == WindowType::companyFaceSelection)
            {
                self.object = nullptr;
                return;
            }

            auto& preferredOwnerFace = Config::get().preferredOwnerFace;

            if (self.object != nullptr)
            {
                // Ensure our temporary object is still loaded
                auto* object = ObjectManager::getTemporaryObject();
                if (object == reinterpret_cast<Object*>(self.object))
                {
                    return;
                }
            }

            ObjectManager::freeTemporaryObject();
            if (ObjectManager::loadTemporaryObject(preferredOwnerFace))
            {
                self.object = reinterpret_cast<std::byte*>(ObjectManager::getTemporaryObject());
            }
            else
            {
                // Can't be loaded? Disable the option
                auto& cfg = Config::get();
                cfg.usePreferredOwnerFace = false;
                cfg.preferredOwnerFace = kEmptyObjectHeader;
                Config::write();

                self.object = nullptr;
            }
        }

        static void prepareDraw(Window& w)
        {
            assert(w.currentTab == Common::tab::company);

            w.activatedWidgets &= ~Common::tabWidgets;
            w.activatedWidgets |= 1ULL << (w.currentTab + 4);

            w.widgets[Common::Widx::frame].right = w.width - 1;
            w.widgets[Common::Widx::frame].bottom = w.height - 1;
            w.widgets[Common::Widx::panel].right = w.width - 1;
            w.widgets[Common::Widx::panel].bottom = w.height - 1;
            w.widgets[Common::Widx::caption].right = w.width - 2;
            w.widgets[Common::Widx::close_button].left = w.width - 15;
            w.widgets[Common::Widx::close_button].right = w.width - 15 + 12;

            if (Config::get().usePreferredOwnerName)
            {
                w.activatedWidgets |= (1ULL << Widx::usePreferredOwnerName);
                w.disabledWidgets &= ~(1ULL << Widx::changeOwnerNameBtn);
            }
            else
            {
                w.activatedWidgets &= ~(1ULL << Widx::usePreferredOwnerName);
                w.disabledWidgets |= (1ULL << Widx::changeOwnerNameBtn);
            }

            if (Config::get().usePreferredOwnerFace)
            {
                w.activatedWidgets |= (1ULL << Widx::usePreferredOwnerFace);
                w.disabledWidgets &= ~((1ULL << Widx::changeOwnerFaceBtn) | (1ULL << Widx::ownerFacePreview));
            }
            else
            {
                w.activatedWidgets &= ~(1ULL << Widx::usePreferredOwnerFace);
                w.disabledWidgets |= ((1ULL << Widx::changeOwnerFaceBtn) | (1ULL << Widx::ownerFacePreview));

                w.widgets[Widx::labelOwnerFace].text = StringIds::empty;
                w.widgets[Widx::ownerFacePreview].content = Widget::kContentNull;

                w.object = nullptr;
            }

            // Set preferred owner name.
            {
                // TODO: Do not share this buffer, also unsafe, we should change the localisation to use a string pointer.
                auto buffer = (char*)StringManager::getString(StringIds::buffer_2039);
                const char* playerName = Config::get().preferredOwnerName.c_str();
                strcpy(buffer, playerName);
                buffer[strlen(playerName)] = '\0';

                FormatArguments args{ w.widgets[Widx::labelPreferredOwnerName].textArgs };
                args.push(StringIds::buffer_2039);
            }

            // Set preferred owner face.
            if (w.object != nullptr)
            {
                const CompetitorObject* competitor = reinterpret_cast<CompetitorObject*>(w.object);

                w.widgets[Widx::labelOwnerFace].text = StringIds::currentPreferredFace;

                FormatArguments args{ w.widgets[Widx::labelOwnerFace].textArgs };
                args.push(competitor->name);

                w.widgets[Widx::ownerFacePreview].image = ImageId(competitor->images[0]).withIndexOffset(1).withPrimary(Colour::black).toUInt32();
            }

            sub_4C13BE(&w);
            loadPreferredFace(w);

            Common::prepareDraw(w);
        }

        static void draw(Window& w, Gfx::DrawingContext& drawingCtx)
        {
            w.draw(drawingCtx);
        }

        // 0x004C1319
        static void changePreferredName(Window* w)
        {
            auto buffer = (char*)StringManager::getString(StringIds::buffer_2039);
            const char* playerName = Config::get().preferredOwnerName.c_str();
            strcpy(buffer, playerName);
            buffer[strlen(playerName)] = '\0';

            TextInput::openTextInput(w, StringIds::preferred_owner_name, StringIds::enter_preferred_owner_name, StringIds::buffer_2039, Widx::usePreferredOwnerName, nullptr);
        }

        // 0x004C135F
        static void usePreferredOwnerNameMouseUp(Window* w)
        {
            auto& cfg = Config::get();
            cfg.usePreferredOwnerName ^= true;
            Config::write();

            w->invalidate();

            if (cfg.usePreferredOwnerName && cfg.preferredOwnerName.empty())
            {
                changePreferredName(w);
            }
        }

        static void changePreferredFace(Window& self)
        {
            CompanyFaceSelection::open(CompanyId::neutral, self.type);
        }

        static void usePreferredOwnerFaceMouseUp(Window* w)
        {
            auto& cfg = Config::get();
            cfg.usePreferredOwnerFace ^= true;
            Config::write();

            w->invalidate();

            if (cfg.usePreferredOwnerFace && cfg.preferredOwnerFace == kEmptyObjectHeader)
            {
                changePreferredFace(*w);
            }
        }

        static void onMouseUp(Window& w, WidgetIndex_t wi, [[maybe_unused]] const WidgetId id)
        {
            switch (wi)
            {
                case Common::Widx::close_button:
                    WindowManager::close(&w);
                    return;

                case Common::Widx::tab_display:
                case Common::Widx::tab_sound:
                case Common::Widx::tab_music:
                case Common::Widx::tab_regional:
                case Common::Widx::tab_controls:
                case Common::Widx::tab_company:
                case Common::Widx::tab_miscellaneous:
                    Options::tabOnMouseUp(&w, wi);
                    return;

                case Widx::usePreferredOwnerName:
                    usePreferredOwnerNameMouseUp(&w);
                    break;

                case Widx::changeOwnerNameBtn:
                    changePreferredName(&w);
                    break;

                case Widx::usePreferredOwnerFace:
                    usePreferredOwnerFaceMouseUp(&w);
                    break;

                case Widx::changeOwnerFaceBtn:
                case Widx::ownerFacePreview:
                    changePreferredFace(w);
                    break;
            }
        }

        // 0x004C1342
        static void setPreferredName(Window* w, const char* str)
        {
            auto& cfg = Config::get();
            cfg.preferredOwnerName = str;
            if (cfg.preferredOwnerName.empty())
            {
                cfg.usePreferredOwnerName = false;
            }

            Config::write();
            w->invalidate();
        }

        // 0x004C1304
        static void textInput(Window& w, WidgetIndex_t i, [[maybe_unused]] const WidgetId id, const char* str)
        {
            switch (i)
            {
                case Widx::usePreferredOwnerName:
                    setPreferredName(&w, str);
                    break;
            }
        }

        // 0x004C139C
        static void onUpdate(Window& w)
        {
            w.frameNo += 1;
            w.callPrepareDraw();
            WindowManager::invalidateWidget(w.type, w.number, w.currentTab + 4);
        }

        static constexpr WindowEventList kEvents = {
            .onClose = Common::onClose,
            .onMouseUp = onMouseUp,
            .onUpdate = onUpdate,
            .textInput = textInput,
            .prepareDraw = prepareDraw,
            .draw = draw,
        };

        static const WindowEventList& getEvents()
        {
            return kEvents;
        }
    }

    namespace Misc
    {
        static constexpr Ui::Size32 kWindowSize = { 420, 266 };

        namespace Widx
        {
            enum
            {
                groupCheats = Common::Widx::tab_miscellaneous + 1,
                enableCheatsToolbarButton,
                disableAICompanies,
                disableTownExpansion,

                groupVehicleBehaviour,
                disable_vehicle_breakdowns,
                disable_vehicle_load_penalty,
                disableStationSizeLimit,
                trainsReverseAtSignals,

                groupSaveOptions,
                autosave_frequency_label,
                autosave_frequency,
                autosave_frequency_btn,
                autosave_amount_label,
                autosave_amount,
                autosave_amount_down_btn,
                autosave_amount_up_btn,
                export_plugin_objects,
            };
        }

        static constexpr auto _widgets = makeWidgets(
            Common::makeCommonWidgets(kWindowSize, StringIds::options_title_miscellaneous),

            // Gameplay tweaks group
            Widgets::GroupBox({ 4, 49 }, { 412, 62 }, WindowColour::secondary, StringIds::gameplay_tweaks),
            Widgets::Checkbox({ 10, 64 }, { 400, 12 }, WindowColour::secondary, StringIds::option_cheat_menu_enable, StringIds::tooltip_option_cheat_menu_enable),
            Widgets::Checkbox({ 10, 79 }, { 400, 12 }, WindowColour::secondary, StringIds::disableAICompanies, StringIds::disableAICompanies_tip),
            Widgets::Checkbox({ 10, 94 }, { 400, 12 }, WindowColour::secondary, StringIds::disableTownExpansion, StringIds::disableTownExpansion_tip),

            // Vehicle behaviour
            Widgets::GroupBox({ 4, 115 }, { 412, 77 }, WindowColour::secondary, StringIds::vehicleTrackBehaviour),
            Widgets::Checkbox({ 10, 130 }, { 400, 12 }, WindowColour::secondary, StringIds::disable_vehicle_breakdowns),
            Widgets::Checkbox({ 10, 145 }, { 200, 12 }, WindowColour::secondary, StringIds::disableVehicleLoadingPenalty, StringIds::disableVehicleLoadingPenaltyTip),
            Widgets::Checkbox({ 10, 160 }, { 200, 12 }, WindowColour::secondary, StringIds::disableStationSizeLimitLabel, StringIds::disableStationSizeLimitTooltip),
            Widgets::Checkbox({ 10, 175 }, { 400, 12 }, WindowColour::secondary, StringIds::trainsReverseAtSignals),

            // Save options group
            Widgets::GroupBox({ 4, 196 }, { 412, 65 }, WindowColour::secondary, StringIds::autosave_preferences),

            Widgets::Label({ 10, 211 }, { 200, 12 }, WindowColour::secondary, ContentAlign::left, StringIds::autosave_frequency),
            Widgets::dropdownWidgets({ 250, 211 }, { 156, 12 }, WindowColour::secondary, StringIds::empty),

            Widgets::Label({ 10, 226 }, { 200, 12 }, WindowColour::secondary, ContentAlign::left, StringIds::autosave_amount),
            Widgets::stepperWidgets({ 250, 226 }, { 156, 12 }, WindowColour::secondary, StringIds::empty),

            Widgets::Checkbox({ 10, 241 }, { 400, 12 }, WindowColour::secondary, StringIds::export_plugin_objects, StringIds::export_plugin_objects_tip)

        );

        static loco_global<uint8_t, 0x0112A17E> _customObjectsInIndex;

        static void enableCheatsToolbarButtonMouseUp(Window* w);
        static void disableVehicleBreakdownsMouseUp(Window* w);
        static void trainsReverseAtSignalsMouseUp(Window* w);
        static void disableAICompaniesMouseUp(Window* w);
        static void disableTownExpansionMouseUp(Window* w);
        static void exportPluginObjectsMouseUp(Window* w);

        // 0x004C11B7
        static void prepareDraw(Window& w)
        {
            assert(w.currentTab == Common::tab::miscellaneous);

            w.activatedWidgets &= ~Common::tabWidgets;
            w.activatedWidgets |= 1ULL << (w.currentTab + 4);

            w.widgets[Common::Widx::frame].right = w.width - 1;
            w.widgets[Common::Widx::frame].bottom = w.height - 1;
            w.widgets[Common::Widx::panel].right = w.width - 1;
            w.widgets[Common::Widx::panel].bottom = w.height - 1;
            w.widgets[Common::Widx::caption].right = w.width - 2;
            w.widgets[Common::Widx::close_button].left = w.width - 15;
            w.widgets[Common::Widx::close_button].right = w.width - 15 + 12;

            if (Config::get().cheatsMenuEnabled)
            {
                w.activatedWidgets |= (1ULL << Widx::enableCheatsToolbarButton);
            }
            else
            {
                w.activatedWidgets &= ~(1ULL << Widx::enableCheatsToolbarButton);
            }

            if (Config::get().breakdownsDisabled)
            {
                w.activatedWidgets |= (1ULL << Widx::disable_vehicle_breakdowns);
            }
            else
            {
                w.activatedWidgets &= ~(1ULL << Widx::disable_vehicle_breakdowns);
            }

            if (Config::get().trainsReverseAtSignals)
            {
                w.activatedWidgets |= (1ULL << Widx::trainsReverseAtSignals);
            }
            else
            {
                w.activatedWidgets &= ~(1ULL << Widx::trainsReverseAtSignals);
            }

            if (Config::get().disableVehicleLoadPenaltyCheat)
            {
                w.activatedWidgets |= (1ULL << Widx::disable_vehicle_load_penalty);
            }
            else
            {
                w.activatedWidgets &= ~(1ULL << Widx::disable_vehicle_load_penalty);
            }

            if (Config::get().disableStationSizeLimit)
            {
                w.activatedWidgets |= (1ULL << Widx::disableStationSizeLimit);
            }
            else
            {
                w.activatedWidgets &= ~(1ULL << Widx::disableStationSizeLimit);
            }

            if (Config::get().companyAIDisabled)
            {
                w.activatedWidgets |= (1ULL << Widx::disableAICompanies);
            }
            else
            {
                w.activatedWidgets &= ~(1ULL << Widx::disableAICompanies);
            }

            if (Config::get().townGrowthDisabled)
            {
                w.activatedWidgets |= (1ULL << Widx::disableTownExpansion);
            }
            else
            {
                w.activatedWidgets &= ~(1ULL << Widx::disableTownExpansion);
            }

            w.activatedWidgets &= ~(1ULL << Widx::export_plugin_objects);
            if (Config::get().hasFlags(Config::Flags::exportObjectsWithSaves))
            {
                w.activatedWidgets |= (1ULL << Widx::export_plugin_objects);
            }

            w.widgets[Widx::export_plugin_objects].hidden = true;
            if (_customObjectsInIndex)
            {
                w.widgets[Widx::export_plugin_objects].hidden = false;
            }

            sub_4C13BE(&w);

            Common::prepareDraw(w);
        }

        static void drawDropdownContent(Window* w, Gfx::DrawingContext& drawingCtx, WidgetIndex_t widgetIndex, StringId stringId, int32_t value)
        {
            auto tr = Gfx::TextRenderer(drawingCtx);

            auto& widget = w->widgets[widgetIndex];
            FormatArguments args{};
            args.push(stringId);
            args.push(value);

            auto point = Point(widget.left + 1, widget.top + 1);
            tr.drawStringLeft(point, Colour::black, StringIds::black_stringid, args);
        }

        // 0x004C1282
        static void draw(Window& w, Gfx::DrawingContext& drawingCtx)
        {
            w.draw(drawingCtx);

            // Value for autosave frequency
            auto freq = Config::get().autosaveFrequency;
            StringId stringId;
            switch (freq)
            {
                case 0:
                    stringId = StringIds::autosave_never;
                    break;
                case 1:
                    stringId = StringIds::autosave_every_month;
                    break;
                default:
                    stringId = StringIds::autosave_every_x_months;
                    break;
            }
            drawDropdownContent(&w, drawingCtx, Widx::autosave_frequency, stringId, freq);

            // Value for autosave amount
            auto scale = Config::get().autosaveAmount;
            drawDropdownContent(&w, drawingCtx, Widx::autosave_amount, StringIds::int_32, scale);
        }

        static void changeAutosaveAmount(Window* w, int32_t delta)
        {
            auto& cfg = Config::get();
            auto newValue = std::clamp(cfg.autosaveAmount + delta, 1, 24);
            if (cfg.autosaveAmount != newValue)
            {
                cfg.autosaveAmount = newValue;
                Config::write();
                w->invalidate();
            }
        }

        static void changeAutosaveFrequency(Window* w, int32_t value)
        {
            auto& cfg = Config::get();
            if (cfg.autosaveFrequency != value)
            {
                cfg.autosaveFrequency = value;
                Config::write();
                w->invalidate();
            }
        }

        static void showAutosaveFrequencyDropdown(Window* w, WidgetIndex_t wi)
        {
            auto dropdown = w->widgets[wi];
            Dropdown::show(w->x + dropdown.left, w->y + dropdown.top, dropdown.width() - 4, dropdown.height(), w->getColour(WindowColour::secondary), 5, 0x80);

            // Add pre-defined entries
            Dropdown::add(0, StringIds::dropdown_stringid, { StringIds::autosave_never });
            Dropdown::add(1, StringIds::dropdown_stringid, { StringIds::autosave_every_month });
            Dropdown::add(2, StringIds::dropdown_stringid, { StringIds::autosave_every_x_months, static_cast<uint32_t>(3) });
            Dropdown::add(3, StringIds::dropdown_stringid, { StringIds::autosave_every_x_months, static_cast<uint32_t>(6) });
            Dropdown::add(4, StringIds::dropdown_stringid, { StringIds::autosave_every_x_months, static_cast<uint32_t>(12) });

            // Set current selection
            auto freq = Config::get().autosaveFrequency;
            std::optional<size_t> selected;
            switch (freq)
            {
                case 0:
                    selected = 0;
                    break;
                case 1:
                    selected = 1;
                    break;
                case 3:
                    selected = 2;
                    break;
                case 6:
                    selected = 3;
                    break;
                case 12:
                    selected = 4;
                    break;
            }
            if (selected)
            {
                Dropdown::setItemSelected(*selected);
            }
        }

        static void handleAutosaveFrequencyDropdown(Window* w, int32_t index)
        {
            switch (index)
            {
                case 0:
                    changeAutosaveFrequency(w, 0);
                    break;
                case 1:
                    changeAutosaveFrequency(w, 1);
                    break;
                case 2:
                    changeAutosaveFrequency(w, 3);
                    break;
                case 3:
                    changeAutosaveFrequency(w, 6);
                    break;
                case 4:
                    changeAutosaveFrequency(w, 12);
                    break;
            }
        }

        // 0x004C12D2
        static void onMouseUp(Window& w, WidgetIndex_t wi, [[maybe_unused]] const WidgetId id)
        {
            switch (wi)
            {
                case Common::Widx::close_button:
                    WindowManager::close(&w);
                    return;

                case Common::Widx::tab_display:
                case Common::Widx::tab_sound:
                case Common::Widx::tab_music:
                case Common::Widx::tab_regional:
                case Common::Widx::tab_controls:
                case Common::Widx::tab_company:
                case Common::Widx::tab_miscellaneous:
                    Options::tabOnMouseUp(&w, wi);
                    return;

                case Widx::enableCheatsToolbarButton:
                    enableCheatsToolbarButtonMouseUp(&w);
                    break;

                case Widx::disable_vehicle_breakdowns:
                    disableVehicleBreakdownsMouseUp(&w);
                    break;

                case Widx::trainsReverseAtSignals:
                    trainsReverseAtSignalsMouseUp(&w);
                    break;

                case Widx::disable_vehicle_load_penalty:
                    Config::get().disableVehicleLoadPenaltyCheat = !Config::get().disableVehicleLoadPenaltyCheat;
                    WindowManager::invalidateWidget(w.type, w.number, Widx::disable_vehicle_load_penalty);
                    break;

                case Widx::disableStationSizeLimit:
                    Config::get().disableStationSizeLimit ^= true;
                    WindowManager::invalidateWidget(w.type, w.number, Widx::disableStationSizeLimit);
                    break;

                case Widx::disableAICompanies:
                    disableAICompaniesMouseUp(&w);
                    break;

                case Widx::disableTownExpansion:
                    disableTownExpansionMouseUp(&w);
                    break;

                case Widx::export_plugin_objects:
                    exportPluginObjectsMouseUp(&w);
                    break;
            }
        }

        static void onMouseDown(Window& w, WidgetIndex_t wi, [[maybe_unused]] const WidgetId id)
        {
            switch (wi)
            {
                case Widx::autosave_frequency_btn:
                    showAutosaveFrequencyDropdown(&w, Widx::autosave_frequency);
                    break;
                case Widx::autosave_amount_down_btn:
                    changeAutosaveAmount(&w, -1);
                    break;
                case Widx::autosave_amount_up_btn:
                    changeAutosaveAmount(&w, 1);
                    break;
            }
        }

        static void onDropdown(Window& w, WidgetIndex_t wi, [[maybe_unused]] const WidgetId id, int16_t item_index)
        {
            switch (wi)
            {
                case Widx::autosave_frequency_btn:
                    handleAutosaveFrequencyDropdown(&w, item_index);
                    break;
            }
        }

        static void enableCheatsToolbarButtonMouseUp(Window* w)
        {
            auto& cfg = OpenLoco::Config::get();
            cfg.cheatsMenuEnabled = !cfg.cheatsMenuEnabled;
            Config::write();
            w->invalidate();
            WindowManager::invalidate(WindowType::topToolbar);
        }

        static void disableVehicleBreakdownsMouseUp(Window* w)
        {
            auto& cfg = OpenLoco::Config::get();
            cfg.breakdownsDisabled = !cfg.breakdownsDisabled;
            Config::write();
            w->invalidate();
        }

        static void trainsReverseAtSignalsMouseUp(Window* w)
        {
            auto& cfg = OpenLoco::Config::get();
            cfg.trainsReverseAtSignals = !cfg.trainsReverseAtSignals;
            Config::write();
            w->invalidate();
        }

        static void disableAICompaniesMouseUp(Window* w)
        {
            auto& cfg = OpenLoco::Config::get();
            cfg.companyAIDisabled = !cfg.companyAIDisabled;
            Config::write();
            w->invalidate();
        }

        static void disableTownExpansionMouseUp(Window* w)
        {
            auto& cfg = OpenLoco::Config::get();
            cfg.townGrowthDisabled = !cfg.townGrowthDisabled;
            Config::write();
            w->invalidate();
        }

        static void exportPluginObjectsMouseUp(Window* w)
        {
            auto& cfg = OpenLoco::Config::get();
            if (cfg.hasFlags(Config::Flags::exportObjectsWithSaves))
            {
                cfg.old.flags &= ~Config::Flags::exportObjectsWithSaves;
            }
            else
            {
                cfg.old.flags |= Config::Flags::exportObjectsWithSaves;
            }
            Config::write();

            w->invalidate();
        }

        // 0x004C139C
        static void onUpdate(Window& w)
        {
            w.frameNo += 1;
            w.callPrepareDraw();
            WindowManager::invalidateWidget(w.type, w.number, w.currentTab + 4);
        }

        static constexpr WindowEventList kEvents = {
            .onClose = Common::onClose,
            .onMouseUp = onMouseUp,
            .onMouseDown = onMouseDown,
            .onDropdown = onDropdown,
            .onUpdate = onUpdate,
            .prepareDraw = prepareDraw,
            .draw = draw,
        };

        static const WindowEventList& getEvents()
        {
            return kEvents;
        }
    }

    static void sub_4BF8CD()
    {
        auto ptr = static_cast<ObjectManager::SelectedObjectsFlags*>(malloc(ObjectManager::getNumInstalledObjects()));
        // TODO: reimplement nullptr check?

        __11364A0 = ptr;
        ObjectManager::markOnlyLoadedObjects(getLoadedSelectedObjectFlags());
    }

    static void sub_4C13BE(Window* w)
    {
        w->disabledWidgets &= ~((1ULL << Common::Widx::tab_music) | (1ULL << Common::Widx::tab_regional));
        if (SceneManager::isEditorMode() || SceneManager::isTitleMode())
        {
            w->disabledWidgets |= 1ULL << Common::Widx::tab_music;
        }

        if (SceneManager::isEditorMode() && Scenario::getOptions().editorStep == EditorController::Step::objectSelection)
        {
            w->disabledWidgets |= 1ULL << Common::Widx::tab_regional;
        }

        Widget::leftAlignTabs(*w, Common::Widx::tab_display, Common::Widx::tab_miscellaneous);
    }

    // 0x004C1519 & 0x00474911
    static void setPreferredCurrencyNameBuffer()
    {
        const auto res = ObjectManager::findObjectInIndex(Config::get().preferredCurrency);
        if (res.has_value())
        {
            auto buffer = const_cast<char*>(StringManager::getString(StringIds::preferred_currency_buffer));
            strcpy(buffer, res->_name.c_str());
        }
    }

    // 0x004BF7B9
    Window* open()
    {
        Window* window;

        window = WindowManager::bringToFront(WindowType::options);
        if (window != nullptr)
        {
            return window;
        }

        // 0x004BF833 (create_options_window)
        window = WindowManager::createWindowCentred(
            WindowType::options,
            Display::kWindowSize,
            WindowFlags::none,
            Display::getEvents());

        window->setWidgets(Display::_widgets);
        window->number = 0;
        window->currentTab = 0;
        window->frameNo = 0;
        window->rowHover = -1;
        window->object = nullptr;

        auto interface = ObjectManager::get<InterfaceSkinObject>();
        window->setColour(WindowColour::primary, interface->windowTitlebarColour);
        window->setColour(WindowColour::secondary, interface->windowOptionsColour);

        sub_4BF8CD();
        populateAvailableCurrencies();
        setPreferredCurrencyNameBuffer();

        Display::applyScreenModeRestrictions(window);

        window->holdableWidgets = 0;
        window->eventHandlers = &Display::getEvents();
        window->activatedWidgets = 0;

        window->callOnResize();
        window->callPrepareDraw();
        window->initScrollWidgets();

        return window;
    }

    // 0x004BF823
    Window* openMusicSettings()
    {
        auto window = open();

        window->callOnMouseUp(Common::Widx::tab_music, window->widgets[Common::Widx::tab_music].id);

        return window;
    }

    struct TabInformation
    {
        std::span<const Widget> widgets;
        const WindowEventList& events;
        Ui::Size32 kWindowSize;
    };

    // clang-format off
    static TabInformation tabInformationByTabOffset[] = {
        { Display::_widgets,  Display::getEvents(),  Display::kWindowSize  },
        { Sound::_widgets,    Sound::getEvents(),    Sound::kWindowSize    },
        { Music::_widgets,    Music::getEvents(),    Music::kWindowSize    },
        { Regional::_widgets, Regional::getEvents(), Regional::kWindowSize },
        { Controls::_widgets, Controls::getEvents(), Controls::kWindowSize },
        { Company::_widgets,  Company::getEvents(),  Company::kWindowSize  },
        { Misc::_widgets,     Misc::getEvents(),     Misc::kWindowSize     },
    };
    // clang-format on

    // 0x004BFC11
    static void tabOnMouseUp(Window* w, WidgetIndex_t wi)
    {
        ToolManager::toolCancel(w->type, w->number);

        TextInput::sub_4CE6C9(w->type, w->number);
        w->currentTab = wi - Common::Widx::tab_display;
        w->frameNo = 0;
        w->flags &= ~(WindowFlags::flag_16);
        w->disabledWidgets = 0;
        w->holdableWidgets = 0;
        w->activatedWidgets = 0;
        w->rowHover = -1;
        w->viewportRemove(0);

        auto& tabInfo = tabInformationByTabOffset[w->currentTab];
        w->eventHandlers = &tabInfo.events;
        w->setWidgets(tabInfo.widgets);
        w->invalidate();
        w->setSize(tabInfo.kWindowSize);

        if ((Common::tab)w->currentTab == Common::tab::display)
        {
            Display::applyScreenModeRestrictions(w);
        }

        else if ((Common::tab)w->currentTab == Common::tab::music)
        {
            w->holdableWidgets = (1ULL << Music::Widx::volume);
        }

        w->callOnResize();
        w->callPrepareDraw();
        w->initScrollWidgets();
        w->invalidate();
    }
}
