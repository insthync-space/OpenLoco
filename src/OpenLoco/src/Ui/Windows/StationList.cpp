#include "Graphics/Colour.h"
#include "Graphics/Gfx.h"
#include "Graphics/ImageIds.h"
#include "Graphics/RenderTarget.h"
#include "Graphics/SoftwareDrawingEngine.h"
#include "Graphics/TextRenderer.h"
#include "Input.h"
#include "Localisation/FormatArguments.hpp"
#include "Localisation/Formatting.h"
#include "Localisation/StringIds.h"
#include "Objects/CargoObject.h"
#include "Objects/CompetitorObject.h"
#include "Objects/InterfaceSkinObject.h"
#include "Objects/ObjectManager.h"
#include "OpenLoco.h"
#include "Ui/Dropdown.h"
#include "Ui/ToolManager.h"
#include "Ui/Widget.h"
#include "Ui/Widgets/CaptionWidget.h"
#include "Ui/Widgets/FrameWidget.h"
#include "Ui/Widgets/ImageButtonWidget.h"
#include "Ui/Widgets/LabelWidget.h"
#include "Ui/Widgets/PanelWidget.h"
#include "Ui/Widgets/ScrollViewWidget.h"
#include "Ui/Widgets/TabWidget.h"
#include "Ui/Widgets/TableHeaderWidget.h"
#include "Ui/WindowManager.h"
#include "World/CompanyManager.h"
#include "World/StationManager.h"
#include "World/TownManager.h"
#include <OpenLoco/Core/Exception.hpp>

namespace OpenLoco::Ui::Windows::StationList
{
    static constexpr Ui::Size32 kWindowSize = { 600, 197 };
    static constexpr Ui::Size kMaxDimensions = { 640, 1200 };
    static constexpr Ui::Size kMinDimensions = { 192, 100 };

    static constexpr uint8_t kRowHeight = 10; // CJK: 13

    enum widx
    {
        frame = 0,
        caption = 1,
        close_button = 2,
        panel = 3,
        tab_all_stations,
        tab_rail_stations,
        tab_road_stations,
        tab_airports,
        tab_ship_ports,
        company_select,
        sort_name,
        sort_status,
        sort_total_waiting,
        sort_accepts,
        scrollview,
        status_bar,
    };

    static constexpr auto _widgets = makeWidgets(
        Widgets::Frame({ 0, 0 }, { 600, 197 }, WindowColour::primary),
        Widgets::Caption({ 1, 1 }, { 598, 13 }, Widgets::Caption::Style::colourText, WindowColour::primary, StringIds::stringid_all_stations),
        Widgets::ImageButton({ 585, 2 }, { 13, 13 }, WindowColour::primary, ImageIds::close_button, StringIds::tooltip_close_window),
        Widgets::Panel({ 0, 41 }, { 600, 155 }, WindowColour::secondary),
        Widgets::Tab({ 3, 15 }, { 31, 27 }, WindowColour::secondary, ImageIds::tab, StringIds::tooltip_all_stations),
        Widgets::Tab({ 3, 15 }, { 31, 27 }, WindowColour::secondary, ImageIds::tab, StringIds::tooltip_rail_stations),
        Widgets::Tab({ 3, 15 }, { 31, 27 }, WindowColour::secondary, ImageIds::tab, StringIds::tooltip_road_stations),
        Widgets::Tab({ 3, 15 }, { 31, 27 }, WindowColour::secondary, ImageIds::tab, StringIds::tooltip_airports),
        Widgets::Tab({ 3, 15 }, { 31, 27 }, WindowColour::secondary, ImageIds::tab, StringIds::tooltip_ship_ports),
        Widgets::ImageButton({ 0, 14 }, { 26, 26 }, WindowColour::primary, Widget::kContentNull, StringIds::tooltip_select_company),
        Widgets::TableHeader({ 4, 43 }, { 200, 12 }, WindowColour::secondary, Widget::kContentNull, StringIds::tooltip_sort_by_name),
        Widgets::TableHeader({ 204, 43 }, { 200, 12 }, WindowColour::secondary, Widget::kContentNull, StringIds::tooltip_sort_by_station_status),
        Widgets::TableHeader({ 404, 43 }, { 90, 12 }, WindowColour::secondary, Widget::kContentNull, StringIds::tooltip_sort_by_total_units_waiting),
        Widgets::TableHeader({ 494, 43 }, { 120, 12 }, WindowColour::secondary, Widget::kContentNull, StringIds::tooltip_sort_by_cargo_accepted),
        Widgets::ScrollView({ 3, 56 }, { 594, 126 }, WindowColour::secondary, Scrollbars::vertical),
        Widgets::Label({ 4, kWindowSize.height - 12 }, { kWindowSize.width, 10 }, WindowColour::secondary, ContentAlign::left, StringIds::black_stringid)

    );

    struct TabDetails
    {
        widx widgetIndex;
        StringId windowTitleId;
        uint32_t imageId;
        StationFlags stationMask;
    };

    static TabDetails tabInformationByType[] = {
        { tab_all_stations, StringIds::stringid_all_stations, InterfaceSkin::ImageIds::all_stations, StationFlags::allModes },
        { tab_rail_stations, StringIds::stringid_rail_stations, InterfaceSkin::ImageIds::rail_stations, StationFlags::transportModeRail },
        { tab_road_stations, StringIds::stringid_road_stations, InterfaceSkin::ImageIds::road_stations, StationFlags::transportModeRoad },
        { tab_airports, StringIds::stringid_airports, InterfaceSkin::ImageIds::airports, StationFlags::transportModeAir },
        { tab_ship_ports, StringIds::stringid_ship_ports, InterfaceSkin::ImageIds::ship_ports, StationFlags::transportModeWater }
    };

    enum SortMode : uint16_t
    {
        Name,
        Status,
        TotalUnitsWaiting,
        CargoAccepted,
    };

    // 0x004910E8
    static void refreshStationList(Window* window)
    {
        window->rowCount = 0;

        for (auto& station : StationManager::stations())
        {
            if (station.owner == CompanyId(window->number))
            {
                station.flags &= ~StationFlags::flag_4;
            }
        }
    }

    // 0x004911FD
    static bool orderByName(const OpenLoco::Station& lhs, const OpenLoco::Station& rhs)
    {
        char lhsString[256] = { 0 };
        {
            auto argsBuf = FormatArgumentsBuffer{};
            auto args = FormatArguments{ argsBuf };
            args.push(lhs.town);
            StringManager::formatString(lhsString, lhs.name, args);
        }

        char rhsString[256] = { 0 };
        {
            auto argsBuf = FormatArgumentsBuffer{};
            auto args = FormatArguments{ argsBuf };
            args.push(rhs.town);
            StringManager::formatString(rhsString, rhs.name, args);
        }

        return strcmp(lhsString, rhsString) < 0;
    }

    // 0x00491281, 0x00491247
    static bool orderByQuantity(const OpenLoco::Station& lhs, const OpenLoco::Station& rhs)
    {
        uint32_t lhsSum = 0;
        for (const auto& cargo : lhs.cargoStats)
        {
            lhsSum += cargo.quantity;
        }

        uint32_t rhsSum = 0;
        for (const auto& cargo : rhs.cargoStats)
        {
            rhsSum += cargo.quantity;
        }

        return rhsSum < lhsSum;
    }

    // 0x004912BB
    static bool orderByAccepts(const OpenLoco::Station& lhs, const OpenLoco::Station& rhs)
    {
        char* ptr;

        char lhsString[256] = { 0 };
        ptr = &lhsString[0];
        for (uint32_t cargoId = 0; cargoId < kMaxCargoStats; cargoId++)
        {
            if (lhs.cargoStats[cargoId].isAccepted())
            {
                ptr = StringManager::formatString(ptr, ObjectManager::get<CargoObject>(cargoId)->name);
            }
        }

        char rhsString[256] = { 0 };
        ptr = &rhsString[0];
        for (uint32_t cargoId = 0; cargoId < kMaxCargoStats; cargoId++)
        {
            if (rhs.cargoStats[cargoId].isAccepted())
            {
                ptr = StringManager::formatString(ptr, ObjectManager::get<CargoObject>(cargoId)->name);
            }
        }

        return strcmp(lhsString, rhsString) < 0;
    }

    // 0x004911FD, 0x00491247, 0x00491281, 0x004912BB
    static bool getOrder(const SortMode mode, const OpenLoco::Station& lhs, const OpenLoco::Station& rhs)
    {
        switch (mode)
        {
            case SortMode::Name:
                return orderByName(lhs, rhs);

            case SortMode::Status:
            case SortMode::TotalUnitsWaiting:
                return orderByQuantity(lhs, rhs);

            case SortMode::CargoAccepted:
                return orderByAccepts(lhs, rhs);
        }

        return false;
    }

    // 0x0049111A
    static void updateStationList(Window* window)
    {
        StationId edi = StationId::null;

        for (auto& station : StationManager::stations())
        {
            if (station.owner != CompanyId(window->number))
            {
                continue;
            }

            if ((station.flags & StationFlags::flag_5) != StationFlags::none)
            {
                continue;
            }

            const StationFlags mask = tabInformationByType[window->currentTab].stationMask;
            if ((station.flags & mask) == StationFlags::none)
            {
                continue;
            }

            if ((station.flags & StationFlags::flag_4) != StationFlags::none)
            {
                continue;
            }

            if (edi == StationId::null)
            {
                edi = station.id();
                continue;
            }

            if (getOrder(SortMode(window->sortMode), station, *StationManager::get(edi)))
            {
                edi = station.id();
            }
        }

        if (edi != StationId::null)
        {
            bool dl = false;

            StationManager::get(edi)->flags |= StationFlags::flag_4;

            auto ebp = window->rowCount;
            if (edi != StationId(window->rowInfo[ebp]))
            {
                window->rowInfo[ebp] = enumValue(edi);
                dl = true;
            }

            window->rowCount += 1;
            if (window->rowCount > window->var_83C)
            {
                window->var_83C = window->rowCount;
                dl = true;
            }

            if (dl)
            {
                window->invalidate();
            }
        }
        else
        {
            if (window->var_83C != window->rowCount)
            {
                window->var_83C = window->rowCount;
                window->invalidate();
            }

            refreshStationList(window);
        }
    }

    // 0x004910AB
    void removeStationFromList(const StationId stationId)
    {
        auto* station = StationManager::get(stationId);
        auto* window = WindowManager::find(WindowType::stationList, enumValue(station->owner));
        if (window != nullptr)
        {
            for (uint16_t i = 0; i < window->var_83C; i++)
            {
                if (stationId == StationId(window->rowInfo[i]))
                {
                    window->rowInfo[i] = enumValue(StationId::null);
                }
            }
        }
    }

    static const WindowEventList& getEvents();

    // 0x00490F6C
    Window* open(CompanyId companyId)
    {
        Window* window = WindowManager::bringToFront(WindowType::stationList, enumValue(companyId));
        if (window != nullptr)
        {
            if (ToolManager::isToolActive(window->type, window->number))
            {
                ToolManager::toolCancel();
            }

            // Still active?
            window = WindowManager::bringToFront(WindowType::stationList, enumValue(companyId));
        }

        if (window == nullptr)
        {
            // 0x00491010
            window = WindowManager::createWindow(
                WindowType::stationList,
                kWindowSize,
                WindowFlags::flag_11,
                getEvents());

            window->number = enumValue(companyId);
            window->owner = companyId;
            window->currentTab = 0;
            window->frameNo = 0;
            window->sortMode = 0;
            window->var_83C = 0;
            window->rowHover = -1;

            refreshStationList(window);

            window->minWidth = kMinDimensions.width;
            window->minHeight = kMinDimensions.height;
            window->maxWidth = kMaxDimensions.width;
            window->maxHeight = kMaxDimensions.height;
            window->flags |= WindowFlags::resizable;

            auto interface = ObjectManager::get<InterfaceSkinObject>();
            window->setColour(WindowColour::secondary, interface->windowPlayerColor);
        }

        window->currentTab = 0;
        window->invalidate();

        window->setWidgets(_widgets);
        window->activatedWidgets = 0;
        window->holdableWidgets = 0;

        window->callOnResize();
        window->callPrepareDraw();
        window->initScrollWidgets();

        return window;
    }

    Window* open(CompanyId companyId, uint8_t type)
    {
        if (type > 4)
        {
            throw Exception::RuntimeError("Unexpected station type");
        }

        Window* stationList = open(companyId);
        widx target = tabInformationByType[type].widgetIndex;
        stationList->callOnMouseUp(target, stationList->widgets[target].id);

        return stationList;
    }

    // 0x004919A4
    static Ui::CursorId cursor(Window& window, WidgetIndex_t widgetIdx, [[maybe_unused]] const WidgetId id, [[maybe_unused]] int16_t xPos, int16_t yPos, Ui::CursorId fallback)
    {
        if (widgetIdx != widx::scrollview)
        {
            return fallback;
        }

        uint16_t currentIndex = yPos / kRowHeight;
        if (currentIndex < window.var_83C && window.rowInfo[currentIndex] != -1)
        {
            return CursorId::handPointer;
        }

        return fallback;
    }

    // 0x0049196F
    static void event_08(Window& window)
    {
        window.flags |= WindowFlags::notScrollView;
    }

    // 0x00491977
    static void event_09(Window& window)
    {
        if (!window.hasFlags(WindowFlags::notScrollView))
        {
            return;
        }

        if (window.rowHover == -1)
        {
            return;
        }

        window.rowHover = -1;
        window.invalidate();
    }

    // 0x00491344
    static void prepareDraw(Ui::Window& window)
    {
        // Reset active tab.
        window.activatedWidgets &= ~((1 << tab_all_stations) | (1 << tab_rail_stations) | (1 << tab_road_stations) | (1 << tab_airports) | (1 << tab_ship_ports));
        window.activatedWidgets |= (1ULL << tabInformationByType[window.currentTab].widgetIndex);

        {
            // Set company name
            auto company = CompanyManager::get(CompanyId(window.number));
            auto args = FormatArguments(window.widgets[widx::caption].textArgs);
            args.push(company->name);
        }

        // Set window title.
        window.widgets[widx::caption].text = tabInformationByType[window.currentTab].windowTitleId;

        // Resize general window widgets.
        window.widgets[widx::frame].right = window.width - 1;
        window.widgets[widx::frame].bottom = window.height - 1;

        window.widgets[widx::panel].right = window.width - 1;
        window.widgets[widx::panel].bottom = window.height - 1;

        window.widgets[widx::caption].right = window.width - 2;

        window.widgets[widx::close_button].left = window.width - 15;
        window.widgets[widx::close_button].right = window.width - 3;

        window.widgets[widx::scrollview].right = window.width - 4;
        window.widgets[widx::scrollview].bottom = window.height - 14;

        // Reposition header buttons.
        window.widgets[widx::sort_name].right = std::min(203, window.width - 4);

        window.widgets[widx::sort_status].left = std::min(204, window.width - 4);
        window.widgets[widx::sort_status].right = std::min(403, window.width - 4);

        window.widgets[widx::sort_total_waiting].left = std::min(404, window.width - 4);
        window.widgets[widx::sort_total_waiting].right = std::min(493, window.width - 4);

        window.widgets[widx::sort_accepts].left = std::min(494, window.width - 4);
        window.widgets[widx::sort_accepts].right = std::min(613, window.width - 4);

        // Reposition company selection.
        window.widgets[widx::company_select].left = window.width - 28;
        window.widgets[widx::company_select].right = window.width - 3;

        // Set header button captions.
        window.widgets[widx::sort_name].text = window.sortMode == SortMode::Name ? StringIds::table_header_name_desc : StringIds::table_header_name;
        window.widgets[widx::sort_status].text = window.sortMode == SortMode::Status ? StringIds::table_header_status_desc : StringIds::table_header_status;
        window.widgets[widx::sort_total_waiting].text = window.sortMode == SortMode::TotalUnitsWaiting ? StringIds::table_header_total_waiting_desc : StringIds::table_header_total_waiting;
        window.widgets[widx::sort_accepts].text = window.sortMode == SortMode::CargoAccepted ? StringIds::table_header_accepts_desc : StringIds::table_header_accepts;

        // Reposition tabs
        Widget::leftAlignTabs(window, widx::tab_all_stations, widx::tab_ship_ports);

        // Reposition status label
        auto& widget = window.widgets[widx::status_bar];
        widget.top = window.height - 12;
        widget.bottom = window.height - 2;

        // TODO: locale-based pluralisation.
        auto args = FormatArguments{ widget.textArgs };
        args.push(window.var_83C == 1 ? StringIds::status_num_stations_singular : StringIds::status_num_stations_plural);
        args.push<uint16_t>(window.var_83C);
    }

    // 0x0049157F
    static void drawScroll(Ui::Window& window, Gfx::DrawingContext& drawingCtx, [[maybe_unused]] const uint32_t scrollIndex)
    {
        const auto& rt = drawingCtx.currentRenderTarget();
        auto tr = Gfx::TextRenderer(drawingCtx);

        auto shade = Colours::getShade(window.getColour(WindowColour::secondary).c(), 4);
        drawingCtx.clearSingle(shade);

        uint16_t yPos = 0;
        for (uint16_t i = 0; i < window.var_83C; i++)
        {
            auto stationId = StationId(window.rowInfo[i]);

            // Skip items outside of view, or irrelevant to the current filter.
            if (yPos + kRowHeight < rt.y || stationId == StationId::null)
            {
                yPos += kRowHeight;
                continue;
            }
            else if (yPos >= yPos + kRowHeight + rt.height)
            {
                break;
            }

            StringId text_colour_id = StringIds::black_stringid;

            // Highlight selection.
            if (stationId == StationId(window.rowHover))
            {
                drawingCtx.drawRect(0, yPos, window.width, kRowHeight, enumValue(ExtColour::unk30), Gfx::RectFlags::transparent);
                text_colour_id = StringIds::wcolour2_stringid;
            }

            auto station = StationManager::get(stationId);

            // First, draw the town name.
            {
                auto args = FormatArguments{};
                args.push(StringIds::stringid_stringid);
                args.push(station->name);
                args.push<uint16_t>(enumValue(station->town));
                args.push<uint16_t>(getTransportIconsFromStationFlags(station->flags));

                auto point = Point(0, yPos);
                tr.drawStringLeftClipped(point, 198, Colour::black, text_colour_id, args);
            }

            char* buffer = const_cast<char*>(StringManager::getString(StringIds::buffer_1250));
            station->getStatusString(buffer);

            // Then the station's current status.
            {
                auto args = FormatArguments{};
                args.push(StringIds::buffer_1250);

                auto point = Point(200, yPos);
                tr.drawStringLeftClipped(point, 198, Colour::black, text_colour_id, args);
            }

            // Total units waiting.
            {
                uint16_t totalUnits = 0;
                for (const auto& stats : station->cargoStats)
                {
                    totalUnits += stats.quantity;
                }

                auto args = FormatArguments{};
                args.push(StringIds::num_units);
                args.push<uint32_t>(totalUnits);

                auto point = Point(400, yPos);
                tr.drawStringLeftClipped(point, 88, Colour::black, text_colour_id, args);
            }

            // And, finally, what goods the station accepts.
            char* ptr = buffer;
            *ptr = '\0';

            for (uint32_t cargoId = 0; cargoId < kMaxCargoStats; cargoId++)
            {
                auto& stats = station->cargoStats[cargoId];

                if (!stats.isAccepted())
                {
                    continue;
                }

                if (*buffer != '\0')
                {
                    ptr = StringManager::formatString(ptr, StringIds::unit_separator);
                }

                ptr = StringManager::formatString(ptr, ObjectManager::get<CargoObject>(cargoId)->name);
            }

            {
                auto args = FormatArguments{};
                args.push(StringIds::buffer_1250);

                auto point = Point(490, yPos);
                tr.drawStringLeftClipped(point, 118, Colour::black, text_colour_id, args);
            }

            yPos += kRowHeight;
        }
    }

    // 00491A76
    static void drawTabs(Ui::Window& self, Gfx::DrawingContext& drawingCtx)
    {
        auto skin = ObjectManager::get<InterfaceSkinObject>();
        auto companyColour = CompanyManager::getCompanyColour(CompanyId(self.number));

        for (const auto& tab : tabInformationByType)
        {
            uint32_t image = Gfx::recolour(skin->img + tab.imageId, companyColour);
            Widget::drawTab(self, drawingCtx, image, tab.widgetIndex);
        }
    }

    // 0x004914D8
    static void draw(Ui::Window& self, Gfx::DrawingContext& drawingCtx)
    {
        // Draw widgets and tabs.
        self.draw(drawingCtx);
        drawTabs(self, drawingCtx);

        // Draw company owner image.
        auto company = CompanyManager::get(CompanyId(self.number));
        auto competitor = ObjectManager::get<CompetitorObject>(company->competitorId);
        uint32_t image = Gfx::recolour(competitor->images[enumValue(company->ownerEmotion)], company->mainColours.primary);
        uint16_t x = self.widgets[widx::company_select].left + 1;
        uint16_t y = self.widgets[widx::company_select].top + 1;
        drawingCtx.drawImage(x, y, image);
    }

    // 0x004917BB
    static void onDropdown(Ui::Window& window, WidgetIndex_t widgetIndex, [[maybe_unused]] const WidgetId id, int16_t itemIndex)
    {
        if (widgetIndex != widx::company_select)
        {
            return;
        }

        if (itemIndex == -1)
        {
            return;
        }

        CompanyId companyId = Dropdown::getCompanyIdFromSelection(itemIndex);

        // Try to find an open station list for this company.
        auto companyWindow = WindowManager::bringToFront(WindowType::stationList, enumValue(companyId));
        if (companyWindow != nullptr)
        {
            return;
        }

        // If not, we'll turn this window into a window for the company selected.
        auto company = CompanyManager::get(companyId);
        if (company->name == StringIds::empty)
        {
            return;
        }

        window.number = enumValue(companyId);
        window.owner = companyId;
        window.sortMode = 0;
        window.rowCount = 0;

        refreshStationList(&window);

        window.var_83C = 0;
        window.rowHover = -1;

        window.callOnResize();
        window.callPrepareDraw();
        window.initScrollWidgets();
        window.invalidate();
    }

    // 0x004917B0
    static void onMouseDown(Ui::Window& window, WidgetIndex_t widgetIndex, [[maybe_unused]] const WidgetId id)
    {
        if (widgetIndex == widx::company_select)
        {
            Dropdown::populateCompanySelect(&window, &window.widgets[widgetIndex]);
        }
    }

    // 0x00491785
    static void onMouseUp(Ui::Window& window, WidgetIndex_t widgetIndex, [[maybe_unused]] const WidgetId id)
    {
        switch (widgetIndex)
        {
            case widx::close_button:
                WindowManager::close(&window);
                break;

            case tab_all_stations:
            case tab_rail_stations:
            case tab_road_stations:
            case tab_airports:
            case tab_ship_ports:
            {
                if (ToolManager::isToolActive(window.type, window.number))
                {
                    ToolManager::toolCancel();
                }

                window.currentTab = widgetIndex - widx::tab_all_stations;
                window.frameNo = 0;

                window.invalidate();

                window.var_83C = 0;
                window.rowHover = -1;

                refreshStationList(&window);

                window.callOnResize();
                window.callPrepareDraw();
                window.initScrollWidgets();
                window.moveInsideScreenEdges();
                break;
            }

            case sort_name:
            case sort_status:
            case sort_total_waiting:
            case sort_accepts:
            {
                auto sortMode = widgetIndex - widx::sort_name;
                if (window.sortMode == sortMode)
                {
                    return;
                }

                window.sortMode = sortMode;
                window.invalidate();
                window.var_83C = 0;
                window.rowHover = -1;

                refreshStationList(&window);
                break;
            }
        }
    }

    // 0x00491A0C
    static void onScrollMouseDown(Ui::Window& window, [[maybe_unused]] int16_t x, int16_t y, [[maybe_unused]] uint8_t scroll_index)
    {
        uint16_t currentRow = y / kRowHeight;
        if (currentRow > window.var_83C)
        {
            return;
        }

        const auto currentStation = StationId(window.rowInfo[currentRow]);
        if (currentStation == StationId::null)
        {
            return;
        }

        Station::open(currentStation);
    }

    // 0x004919D1
    static void onScrollMouseOver(Ui::Window& window, [[maybe_unused]] int16_t x, int16_t y, [[maybe_unused]] uint8_t scroll_index)
    {
        window.flags &= ~(WindowFlags::notScrollView);

        uint16_t currentRow = y / kRowHeight;
        int16_t currentStation = -1;

        if (currentRow < window.var_83C)
        {
            currentStation = window.rowInfo[currentRow];
        }

        if (currentStation == window.rowHover)
        {
            return;
        }

        window.rowHover = currentStation;
        window.invalidate();
    }

    // 0x0049193F
    static void onUpdate(Window& window)
    {
        window.frameNo++;

        window.callPrepareDraw();
        WindowManager::invalidateWidget(WindowType::stationList, window.number, window.currentTab + 4);

        // Add three stations every tick.
        updateStationList(&window);
        updateStationList(&window);
        updateStationList(&window);
    }

    // 0x00491999
    static void getScrollSize(Ui::Window& window, [[maybe_unused]] uint32_t scrollIndex, [[maybe_unused]] uint16_t* scrollWidth, uint16_t* scrollHeight)
    {
        *scrollHeight = kRowHeight * window.var_83C;
    }

    // 0x00491841
    static std::optional<FormatArguments> tooltip([[maybe_unused]] Ui::Window& window, [[maybe_unused]] WidgetIndex_t widgetIndex, [[maybe_unused]] const WidgetId id)
    {
        FormatArguments args{};
        args.push(StringIds::tooltip_scroll_station_list);
        return args;
    }

    static constexpr WindowEventList kEvents = {
        .onMouseUp = onMouseUp,
        .onMouseDown = onMouseDown,
        .onDropdown = onDropdown,
        .onUpdate = onUpdate,
        .event_08 = event_08,
        .event_09 = event_09,
        .getScrollSize = getScrollSize,
        .scrollMouseDown = onScrollMouseDown,
        .scrollMouseOver = onScrollMouseOver,
        .tooltip = tooltip,
        .cursor = cursor,
        .prepareDraw = prepareDraw,
        .draw = draw,
        .drawScroll = drawScroll,
    };

    static const WindowEventList& getEvents()
    {
        return kEvents;
    }
}
