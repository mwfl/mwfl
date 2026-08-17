#include <mwfl/mwfl.h>

#include <windows.h>
#include <commctrl.h>
#include <prsht.h>

#include <array>
#include <cstdlib>
#include <stdexcept>
#include <thread>

namespace {

void PumpPendingMessages() {
    MSG message{};
    while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }
}

}  // namespace

int main() {
    using namespace mwfl;
    using mwfl::operator""_dip;

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES};
    if (::InitCommonControlsEx(&controls) == FALSE) return 1;

    bool valid = true;
    int apply_count = 0;
    int reset_count = 0;
    int command_count = 0;
    Label general_label;
    Label advanced_label;
    PropertyPage* general_page = nullptr;
    PropertyPage* advanced_page = nullptr;
    std::array pages{
        PropertyPage({
            {1},
            L"General",
            {
                .initialize =
                    [&](HWND page) {
                        return general_label.Create(page, {101}, L"General settings", RectDip{}) &&
                               general_page->SetLayout(
                                   Column().Margin(12.0_dip).Add(general_label, Stretch()));
                    },
                .command =
                    [&](HWND, WORD id, WORD notification) {
                        if (id != 101 || notification != STN_CLICKED) return false;
                        ++command_count;
                        return true;
                    },
                .validate =
                    [&](HWND) {
                        return valid ? PropertyPageValidation::valid
                                     : PropertyPageValidation::invalid;
                    },
                .apply =
                    [&](HWND) {
                        ++apply_count;
                        return true;
                    },
                .reset = [&](HWND) { ++reset_count; },
            },
        }),
        PropertyPage({
            {2},
            L"Advanced",
            {
                .initialize =
                    [&](HWND page) {
                        return advanced_label.Create(page, {102}, L"Advanced settings",
                                                     RectDip{}) &&
                               advanced_page->SetLayout(
                                   Column().Margin(12.0_dip).Add(advanced_label, Stretch()));
                    },
                .validate = [](HWND) { return PropertyPageValidation::valid; },
                .apply =
                    [&](HWND) {
                        ++apply_count;
                        return true;
                    },
                .reset = [&](HWND) { ++reset_count; },
            },
        }),
    };
    general_page = &pages[0];
    advanced_page = &pages[1];

    PropertySheetDialog sheet;
    if (!sheet.CreateModeless({.title = L"mwfl property sheet", .start_page = PropertyPageId{1}},
                              pages) ||
        !sheet.IsWindow() || !sheet.IsOwnerThread() || pages[0].GetHwnd() == nullptr ||
        pages[1].GetHwnd() != nullptr)
        return 2;

    const HWND native_sheet = sheet.GetHwnd();
    const HWND tabs = PropSheet_GetTabControl(native_sheet);
    if (tabs == nullptr || TabCtrl_GetItemCount(tabs) != 2 ||
        (::GetWindowLongPtrW(native_sheet, GWL_STYLE) & WS_THICKFRAME) == 0)
        return 3;
    TCITEMW item{};
    wchar_t title[32]{};
    item.mask = TCIF_TEXT;
    item.pszText = title;
    item.cchTextMax = 32;
    if (TabCtrl_GetItem(tabs, 0, &item) == FALSE || std::wstring_view(title) != L"General")
        return 4;
    ::SendMessageW(pages[0].GetHwnd(), WM_COMMAND, MAKEWPARAM(101, STN_CLICKED),
                   reinterpret_cast<LPARAM>(general_label.GetHwnd()));
    if (command_count != 1) return 17;

    RECT sheet_bounds{};
    RECT page_before{};
    ::GetWindowRect(native_sheet, &sheet_bounds);
    ::GetClientRect(pages[0].GetHwnd(), &page_before);
    if (::SetWindowPos(native_sheet, nullptr, 0, 0, sheet_bounds.right - sheet_bounds.left + 120,
                       sheet_bounds.bottom - sheet_bounds.top + 80,
                       SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER) == FALSE)
        return 5;
    PumpPendingMessages();
    RECT page_after{};
    ::GetClientRect(pages[0].GetHwnd(), &page_after);
    if (page_after.right <= page_before.right || page_after.bottom <= page_before.bottom) return 6;

    // comctl32's Apply button identifier; kept local so no framework provides
    // this value as ID_APPLY_NOW.
    constexpr int kApplyButtonId = 0x3021;
    const HWND apply_button = ::GetDlgItem(native_sheet, kApplyButtonId);
    if (apply_button == nullptr || !pages[0].SetDirty() || ::IsWindowEnabled(apply_button) == FALSE)
        return 7;
    PropSheet_PressButton(native_sheet, PSBTN_APPLYNOW);
    PumpPendingMessages();
    if (apply_count != 1 || pages[0].IsDirty() || ::IsWindowEnabled(apply_button) != FALSE)
        return 8;

    valid = false;
    if (!pages[0].SetDirty()) return 9;
    PropSheet_PressButton(native_sheet, PSBTN_APPLYNOW);
    PumpPendingMessages();
    if (!sheet.IsWindow() || apply_count != 1 || !pages[0].IsDirty()) return 10;
    valid = true;
    PropSheet_PressButton(native_sheet, PSBTN_APPLYNOW);
    PumpPendingMessages();
    if (apply_count != 2 || pages[0].IsDirty()) return 11;

    if (PropSheet_SetCurSel(native_sheet, nullptr, 1) == FALSE || pages[1].GetHwnd() == nullptr ||
        pages[0].GetHwnd() == nullptr)
        return 12;
    RECT advanced_bounds{};
    RECT label_bounds{};
    ::GetClientRect(pages[1].GetHwnd(), &advanced_bounds);
    ::GetWindowRect(advanced_label.GetHwnd(), &label_bounds);
    ::MapWindowPoints(nullptr, pages[1].GetHwnd(), reinterpret_cast<POINT*>(&label_bounds), 2);
    if (label_bounds.left <= 0 || label_bounds.top <= 0 ||
        label_bounds.right > advanced_bounds.right)
        return 13;

    PropSheet_PressButton(native_sheet, PSBTN_CANCEL);
    PumpPendingMessages();
    if (sheet.IsWindow() || sheet.GetResult().status != PropertySheetStatus::cancelled ||
        reset_count < 1)
        return 14;
    sheet.Close();

    int modal_apply = 0;
    PropertyPage modal_page({
        {10},
        L"Modal",
        {
            .initialize =
                [](HWND page) {
                    return ::PostMessageW(::GetParent(page), PSM_PRESSBUTTON, PSBTN_OK, 0) != FALSE;
                },
            .validate = [](HWND) { return PropertyPageValidation::valid; },
            .apply =
                [&](HWND) {
                    ++modal_apply;
                    return true;
                },
        },
    });
    std::array modal_pages{std::move(modal_page)};
    const PropertySheetResult modal =
        PropertySheetDialog::ShowModal({.title = L"Modal property sheet"}, modal_pages);
    if (modal.status != PropertySheetStatus::accepted || modal_apply != 1 ||
        modal.callback_exception)
        return 15;

    PropertyPage throwing_page({
        {20},
        L"Throwing",
        {
            .initialize =
                [](HWND page) {
                    return ::PostMessageW(::GetParent(page), PSM_PRESSBUTTON, PSBTN_OK, 0) != FALSE;
                },
            .validate = [](HWND) -> PropertyPageValidation {
                throw std::runtime_error("validation failure");
            },
        },
    });
    std::array throwing_pages{std::move(throwing_page)};
    const PropertySheetResult throwing =
        PropertySheetDialog::ShowModal({.title = L"Throwing property sheet"}, throwing_pages);
    if (throwing.status != PropertySheetStatus::failed || !throwing.callback_exception) return 16;

    PropertySheetDialog invalid_sheet;
    if (invalid_sheet.CreateModeless({}, std::span<PropertyPage>{}) ||
        ::GetLastError() != ERROR_INVALID_PARAMETER)
        return 18;

    PropertyPage duplicate_first({{30}, L"First", {}});
    PropertyPage duplicate_second({{30}, L"Second", {}});
    std::array duplicate_pages{std::move(duplicate_first), std::move(duplicate_second)};
    if (invalid_sheet.CreateModeless({}, duplicate_pages) ||
        ::GetLastError() != ERROR_INVALID_PARAMETER)
        return 19;

    PropertyPage missing_start_page({{31}, L"Only page", {}});
    std::array missing_start_pages{std::move(missing_start_page)};
    if (invalid_sheet.CreateModeless({.start_page = PropertyPageId{99}}, missing_start_pages) ||
        ::GetLastError() != ERROR_INVALID_PARAMETER)
        return 26;

    PropertyPage apply_failure_page({
        {40},
        L"Apply failure",
        {
            .initialize =
                [](HWND page) {
                    const HWND sheet = ::GetParent(page);
                    return ::PostMessageW(sheet, PSM_PRESSBUTTON, PSBTN_OK, 0) != FALSE &&
                           ::PostMessageW(sheet, PSM_PRESSBUTTON, PSBTN_CANCEL, 0) != FALSE;
                },
            .validate = [](HWND) { return PropertyPageValidation::valid; },
            .apply = [](HWND) { return false; },
        },
    });
    std::array apply_failure_pages{std::move(apply_failure_page)};
    const PropertySheetResult apply_failure =
        PropertySheetDialog::ShowModal({.title = L"Apply failure"}, apply_failure_pages);
    if (apply_failure.status != PropertySheetStatus::failed ||
        apply_failure.error != ERROR_FUNCTION_FAILED)
        return 20;

    const DWORD user_before = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    const DWORD gdi_before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    for (std::uint64_t iteration = 0; iteration < 40; ++iteration) {
        PropertyPage stress_page({
            {1000 + iteration},
            L"Stress",
            {.initialize = [](HWND) { return true; }},
        });
        std::array stress_pages{std::move(stress_page)};
        PropertySheetDialog stress_sheet;
        if (!stress_sheet.CreateModeless({.title = L"Stress"}, stress_pages)) return 21;
        stress_sheet.Close();
        PumpPendingMessages();
        if (stress_sheet.IsWindow()) return 22;
    }
    const DWORD user_after = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    const DWORD gdi_after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    if (user_after > user_before + 8 || gdi_after > gdi_before + 8) return 23;

    PropertyPage cross_thread_page({
        {2000},
        L"Cross-thread close",
        {.initialize = [](HWND) { return true; }},
    });
    std::array cross_thread_pages{std::move(cross_thread_page)};
    PropertySheetDialog cross_thread_sheet;
    if (!cross_thread_sheet.CreateModeless({.title = L"Cross-thread close"}, cross_thread_pages))
        return 24;
    const HWND cross_thread_window = cross_thread_sheet.GetHwnd();
    std::thread closer([sheet = std::move(cross_thread_sheet)]() mutable {
        if (sheet.IsOwnerThread()) std::terminate();
        sheet.Close();
    });
    closer.join();
    PumpPendingMessages();
    if (::IsWindow(cross_thread_window) != FALSE) return 25;

    return EXIT_SUCCESS;
}
