#include <mwtl/scintilla.h>

#include <oleacc.h>

namespace {
struct Notifications {
    mwtl::ScintillaEditor* editor = nullptr;
    int modified = 0;
    int save_point_left = 0;
    int save_point_reached = 0;
};

LRESULT CALLBACK ParentSubclass(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                UINT_PTR id, DWORD_PTR reference) noexcept {
    auto* state = reinterpret_cast<Notifications*>(reference);
    if (message == WM_NOTIFY && state && state->editor) {
        const auto* header = reinterpret_cast<const NMHDR*>(lparam);
        if (header) {
            if (const auto decoded = state->editor->DecodeNotification(*header)) {
                if (decoded->kind == mwtl::ScintillaNotificationKind::modified) ++state->modified;
                if (decoded->kind == mwtl::ScintillaNotificationKind::save_point_left)
                    ++state->save_point_left;
                if (decoded->kind == mwtl::ScintillaNotificationKind::save_point_reached)
                    ++state->save_point_reached;
            }
        }
    }
    if (message == WM_NCDESTROY) ::RemoveWindowSubclass(window, ParentSubclass, id);
    return ::DefSubclassProc(window, message, wparam, lparam);
}

bool HasAccessibleName(HWND window) {
    IAccessible* accessible = nullptr;
    if (FAILED(::AccessibleObjectFromWindow(window, static_cast<DWORD>(OBJID_CLIENT), IID_IAccessible,
                                            reinterpret_cast<void**>(&accessible))))
        return false;
    VARIANT child{};
    child.vt = VT_I4;
    child.lVal = CHILDID_SELF;
    BSTR name = nullptr;
    const bool result = SUCCEEDED(accessible->get_accName(child, &name)) && name != nullptr;
    ::SysFreeString(name);
    accessible->Release();
    return result;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    using namespace mwtl;
    ScintillaRuntime missing;
    if (missing.Load(L"definitely-missing-Scintilla.dll") ||
        missing.GetStatus() != ScintillaRuntimeStatus::missing)
        return 1;

    ScintillaRuntime runtime;
    if (!runtime.LoadAdjacent() || !runtime.IsAvailable() ||
        runtime.GetLoadedPath().filename() != L"Scintilla.dll")
        return 2;
    const HWND parent = ::CreateWindowExW(0, L"STATIC", L"editor-test",
                                           WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 500, 360,
                                           nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    if (!parent) return 3;
    Notifications notifications;
    ::SetWindowSubclass(parent, ParentSubclass, 1, reinterpret_cast<DWORD_PTR>(&notifications));

    ScintillaEditor editor;
    notifications.editor = &editor;
    if (!editor.Create(parent, {731}, {0.0_dip, 0.0_dip, 420.0_dip, 260.0_dip}, runtime) ||
        !HasAccessibleName(editor.GetHwnd()) || !editor.ConfigureCodeEditing())
        return 4;
    runtime.Reset();
    if (!editor.SetText(L"alpha 世界\nbeta alpha\n") || editor.GetText() != L"alpha 世界\nbeta alpha\n")
        return 5;
    if (!editor.IsModified() || notifications.modified == 0 || notifications.save_point_left == 0)
        return 6;
    editor.SetSavePoint();
    if (editor.IsModified() || notifications.save_point_reached == 0) return 7;

    const auto found = editor.Find(L"世界");
    if (!found || found->start != 6 || found->end != 12 || !editor.SetSelection(*found) ||
        editor.GetSelection().start != found->start)
        return 8;
    const auto second_alpha = editor.Find(L"alpha", 1, -1, ScintillaSearchFlags::match_case);
    if (!second_alpha || !editor.ReplaceTarget(L"omega") ||
        editor.GetText() != L"alpha 世界\nbeta omega\n")
        return 9;
    if (!editor.CanUndo()) return 10;
    editor.Undo();
    if (!editor.CanRedo() || editor.GetText() != L"alpha 世界\nbeta alpha\n") return 11;
    editor.Redo();
    if (editor.GetText() != L"alpha 世界\nbeta omega\n") return 12;
    if (!editor.SetReadOnly(true) || !editor.IsReadOnly() || !editor.SetReadOnly(false)) return 13;
    editor.SetZoom(2);
    if (editor.GetZoom() != 2 || !editor.UpdateLineNumberMargin()) return 14;

    RECT client{};
    ::GetClientRect(editor.GetHwnd(), &client);
    if (client.right <= 0 || client.bottom <= 0) return 15;
    ::SetFocus(editor.GetHwnd());
    if (::GetFocus() != editor.GetHwnd()) return 16;

    for (int iteration = 0; iteration < 40; ++iteration) {
        ScintillaRuntime transient_runtime;
        if (!transient_runtime.LoadAdjacent()) return 17;
        ScintillaEditor transient;
        if (!transient.Create(parent, {800 + iteration},
                              {0.0_dip, 0.0_dip, 40.0_dip, 30.0_dip}, transient_runtime) ||
            !transient.SetText(L"stress"))
            return 18;
        transient_runtime.Reset();
        transient.Destroy();
    }

    notifications.editor = nullptr;
    editor.Destroy();
    ::DestroyWindow(parent);
    return 0;
}
