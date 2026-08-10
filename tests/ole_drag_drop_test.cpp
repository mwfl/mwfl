#include <mwtl/ole_drag_drop.h>

#include <atomic>
#include <thread>

namespace {
LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    return ::DefWindowProcW(window, message, wparam, lparam);
}
}

int main() {
    using namespace mwtl;
    if (FAILED(::OleInitialize(nullptr))) return 1;
    struct Uninitialize { ~Uninitialize() { ::OleUninitialize(); } } uninitialize;

    OleDataObjectBuilder builder;
    if (!builder.AddUnicodeText(L"drag payload")) return 2;
    auto data = builder.Build();
    int entered = 0, over = 0, left = 0, dropped = 0;
    auto target = CreateOleDropTarget({
        .enter = [&](IDataObject& object, DWORD, POINTL, DWORD) {
            ++entered;
            return ReadOleUnicodeText(object).text == L"drag payload"
                       ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        },
        .over = [&](DWORD, POINTL, DWORD) { ++over; return DROPEFFECT_MOVE; },
        .leave = [&] { ++left; },
        .drop = [&](IDataObject& object, DWORD, POINTL, DWORD) {
            ++dropped;
            return ReadOleUnicodeText(object) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        }});
    if (!target) return 3;
    DWORD effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
    if (FAILED(target->DragEnter(data.Get(), 0, {10, 20}, &effect)) ||
        effect != DROPEFFECT_COPY || entered != 1) return 4;
    effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
    if (FAILED(target->DragOver(MK_SHIFT, {11, 21}, &effect)) ||
        effect != DROPEFFECT_MOVE || over != 1) return 5;
    if (target->DragLeave() != S_OK || left != 1 || target->DragLeave() != S_FALSE) return 6;
    effect = DROPEFFECT_COPY;
    if (FAILED(target->DragEnter(data.Get(), 0, {}, &effect)) ||
        FAILED(target->Drop(data.Get(), 0, {}, &effect)) || dropped != 1) return 7;

    auto source = CreateOleDropSource();
    if (!source || source->QueryContinueDrag(TRUE, MK_LBUTTON) != DRAGDROP_S_CANCEL ||
        source->QueryContinueDrag(FALSE, 0) != DRAGDROP_S_DROP ||
        source->QueryContinueDrag(FALSE, MK_LBUTTON) != S_OK ||
        source->GiveFeedback(DROPEFFECT_COPY) != DRAGDROP_S_USEDEFAULTCURSORS) return 8;

    WNDCLASSW type{};
    type.lpfnWndProc = WindowProcedure;
    type.hInstance = ::GetModuleHandleW(nullptr);
    type.lpszClassName = L"mwtl.OleDropTargetTest";
    if (!::RegisterClassW(&type) && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 9;
    HWND window = ::CreateWindowExW(0, type.lpszClassName, L"", WS_OVERLAPPED,
                                     0, 0, 100, 100, nullptr, nullptr, type.hInstance, nullptr);
    if (!window) return 10;
    auto registered = RegisterOleDropTarget(window, target);
    if (!registered || !registered.registration.IsRegistered()) return 11;
    auto duplicate = RegisterOleDropTarget(window, target);
    if (duplicate.status != OleDragStatus::already_registered) return 12;

    std::atomic<OleDragStatus> wrong_thread{OleDragStatus::success};
    std::thread worker([&] { wrong_thread = registered.registration.Revoke().status; });
    worker.join();
    if (wrong_thread != OleDragStatus::wrong_thread ||
        !registered.registration.IsRegistered()) return 13;
    if (!registered.registration.Revoke() || registered.registration.IsRegistered() ||
        registered.registration.Revoke().status != OleDragStatus::not_registered) return 14;
    ::DestroyWindow(window);

    HWND destroyed_window = ::CreateWindowExW(0, type.lpszClassName, L"", WS_OVERLAPPED,
                                                0, 0, 100, 100, nullptr, nullptr,
                                                type.hInstance, nullptr);
    auto destroyed_registration = RegisterOleDropTarget(destroyed_window, target);
    if (!destroyed_registration || !::DestroyWindow(destroyed_window)) return 16;
    if (destroyed_registration.registration.Revoke().status !=
        OleDragStatus::window_destroyed) return 17;

    std::atomic<OleDragStatus> mta_status{OleDragStatus::success};
    HWND apartment_window = ::CreateWindowExW(0, type.lpszClassName, L"", WS_OVERLAPPED,
                                               0, 0, 100, 100, nullptr, nullptr,
                                               type.hInstance, nullptr);
    std::thread mta([&] {
        const HRESULT initialized = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        auto local_target = CreateOleDropTarget({});
        mta_status = RegisterOleDropTarget(apartment_window, local_target).status;
        if (SUCCEEDED(initialized)) ::CoUninitialize();
    });
    mta.join();
    ::DestroyWindow(apartment_window);
    if (mta_status != OleDragStatus::wrong_apartment) return 18;
    if (DoOleDragDrop(*data.Get(), DROPEFFECT_NONE).status !=
        OleDragStatus::invalid_argument) return 19;

    auto throwing = CreateOleDropTarget({
        .enter = [](IDataObject&, DWORD, POINTL, DWORD) -> DWORD { throw 1; }});
    effect = DROPEFFECT_COPY;
    if (throwing->DragEnter(data.Get(), 0, {}, &effect) != E_UNEXPECTED ||
        effect != DROPEFFECT_NONE) return 15;

    HWND closing_window = ::CreateWindowExW(0, type.lpszClassName, L"", WS_OVERLAPPED,
                                              0, 0, 100, 100, nullptr, nullptr,
                                              type.hInstance, nullptr);
    if (!closing_window) return 20;
    auto closing_target = CreateOleDropTarget({
        .enter = [&](IDataObject&, DWORD, POINTL, DWORD) {
            if (::IsWindow(closing_window)) ::DestroyWindow(closing_window);
            return DROPEFFECT_NONE;
        }});
    auto closing_registration = RegisterOleDropTarget(closing_window, closing_target);
    if (!closing_registration) return 21;
    effect = DROPEFFECT_COPY;
    if (closing_target->DragEnter(data.Get(), 0, {}, &effect) != S_OK ||
        ::IsWindow(closing_window) || effect != DROPEFFECT_NONE ||
        closing_registration.registration.Revoke().status != OleDragStatus::window_destroyed)
        return 22;
    return 0;
}
