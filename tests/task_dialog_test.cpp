#include <mwtl/control_resources.h>

#include <windows.h>

#include <cstdlib>
#include <stdexcept>

int main() {
    using namespace mwtl;

    if (!IsTaskDialogAvailable()) return 1;
    const TaskDialogResult invalid =
        ShowTaskDialog({.buttons = {{0, L"Invalid button"}}});
    if (invalid.status != E_INVALIDARG) return 2;
    const TaskDialogResult duplicate =
        ShowTaskDialog({.buttons = {{10, L"First"}, {10, L"Duplicate"}}});
    if (duplicate.status != E_INVALIDARG) return 6;
    const TaskDialogResult common_collision = ShowTaskDialog(
        {.common_buttons = TDCBF_CANCEL_BUTTON, .buttons = {{IDCANCEL, L"Collision"}}});
    if (common_collision.status != E_INVALIDARG) return 8;
    const TaskDialogResult missing_button =
        ShowTaskDialog({.buttons = {{10, L"Known"}}, .default_button = 11});
    if (missing_button.status != E_INVALIDARG) return 9;
    const TaskDialogResult missing_radio = ShowTaskDialog(
        {.radio_buttons = {{20, L"Known"}}, .default_radio_button = 21});
    if (missing_radio.status != E_INVALIDARG) return 7;

    int created = 0;
    int radio_events = 0;
    int verification_events = 0;
    int button_events = 0;
    TaskDialogOptions options{
        .title = L"mwtl Task Dialog test",
        .main_instruction = L"Structured Task Dialog",
        .content = L"This dialog closes itself through the public controller.",
        .verification_text = L"Remember this choice",
        .expanded_information = L"Expanded details",
        .expanded_control_text = L"Show details",
        .collapsed_control_text = L"Hide details",
        .footer = L"Native TaskDialogIndirect integration",
        .common_buttons = 0,
        .flags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SHOW_PROGRESS_BAR,
        .buttons = {{100, L"Complete"}},
        .radio_buttons = {{200, L"Fast"}, {201, L"Thorough"}},
        .default_button = 100,
        .default_radio_button = 200,
        .callback = [&](const TaskDialogEvent& event, const TaskDialogController& controller) {
            if (event.kind == TaskDialogEventKind::created) {
                ++created;
                if (!controller.SetProgressRange(0, 100) ||
                    !controller.SetProgressPosition(65) ||
                    !controller.SetProgressState(PBST_NORMAL) ||
                    !controller.EnableButton(100, true) ||
                    !controller.EnableRadioButton(201, true) ||
                    !controller.ClickRadioButton(201) ||
                    !controller.ClickVerification(true) ||
                    !controller.SetElementText(TDE_CONTENT, L"Controller update succeeded") ||
                    !controller.ClickButton(100)) {
                    throw std::runtime_error("Task Dialog controller failure");
                }
            } else if (event.kind == TaskDialogEventKind::radio_button_clicked) {
                ++radio_events;
            } else if (event.kind == TaskDialogEventKind::verification_clicked) {
                ++verification_events;
            } else if (event.kind == TaskDialogEventKind::button_clicked) {
                ++button_events;
                if (button_events == 1) {
                    ::PostMessageW(event.window, TDM_CLICK_BUTTON, 100, 0);
                    return TaskDialogCallbackResult::keep_open;
                }
            }
            return TaskDialogCallbackResult::continue_default;
        },
    };
    const TaskDialogResult result = ShowTaskDialog(options);
    if (!result || result.button != 100 || result.radio_button != 201 ||
        !result.verification_checked || result.callback_exception || created != 1 ||
        radio_events < 1 || verification_events < 1 || button_events != 2)
        return 3;

    const TaskDialogResult cancelled = ShowTaskDialog({
        .title = L"Cancel test",
        .content = L"Automatic cancellation",
        .common_buttons = TDCBF_CANCEL_BUTTON,
        .callback = [](const TaskDialogEvent& event, const TaskDialogController& controller) {
            if (event.kind == TaskDialogEventKind::created) controller.ClickButton(IDCANCEL);
            return TaskDialogCallbackResult::continue_default;
        },
    });
    if (!cancelled || cancelled.button != IDCANCEL) return 4;

    const TaskDialogResult throwing = ShowTaskDialog({
        .title = L"Exception test",
        .common_buttons = TDCBF_CANCEL_BUTTON,
        .callback = [](const TaskDialogEvent& event,
                       const TaskDialogController&) -> TaskDialogCallbackResult {
            if (event.kind == TaskDialogEventKind::created) {
                throw std::runtime_error("callback failure");
            }
            return TaskDialogCallbackResult::continue_default;
        },
    });
    if (throwing.status != E_UNEXPECTED || !throwing.callback_exception) return 5;

    const DWORD user_before = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    const DWORD gdi_before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    for (int iteration = 0; iteration < 30; ++iteration) {
        const TaskDialogResult stress = ShowTaskDialog({
            .title = L"Task Dialog stress",
            .common_buttons = TDCBF_OK_BUTTON,
            .callback = [](const TaskDialogEvent& event,
                           const TaskDialogController& controller) {
                if (event.kind == TaskDialogEventKind::created) controller.ClickButton(IDOK);
                return TaskDialogCallbackResult::continue_default;
            },
        });
        if (!stress || stress.button != IDOK) return 10;
    }
    const DWORD user_after = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    const DWORD gdi_after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    if (user_after > user_before + 8 || gdi_after > gdi_before + 8) return 11;

    return EXIT_SUCCESS;
}
