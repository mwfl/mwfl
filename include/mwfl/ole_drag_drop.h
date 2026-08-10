#pragma once

#include <windows.h>
#include <oleidl.h>
#include <wrl/client.h>

#include <functional>

#include <mwfl/ole_data.h>

namespace mwfl {

enum class OleDragStatus {
    success,
    cancelled,
    invalid_argument,
    wrong_apartment,
    wrong_thread,
    already_registered,
    not_registered,
    window_destroyed,
    com_failure,
};

struct OleDragResult {
    DWORD effect = DROPEFFECT_NONE;
    OleDragStatus status = OleDragStatus::com_failure;
    HRESULT error = E_FAIL;
    explicit operator bool() const noexcept { return status == OleDragStatus::success; }
};

struct OleDropTargetCallbacks {
    // Return a preferred single effect. The implementation intersects it with
    // source-allowed effects and modifier keys before returning to OLE.
    std::function<DWORD(IDataObject&, DWORD key_state, POINTL position,
                        DWORD allowed_effects)> enter;
    std::function<DWORD(DWORD key_state, POINTL position, DWORD allowed_effects)> over;
    std::function<void()> leave;
    std::function<DWORD(IDataObject&, DWORD key_state, POINTL position,
                        DWORD allowed_effects)> drop;
};

Microsoft::WRL::ComPtr<IDropSource> CreateOleDropSource();
Microsoft::WRL::ComPtr<IDropTarget> CreateOleDropTarget(OleDropTargetCallbacks callbacks);

// Runs the modal OLE drag loop on the current OLE STA. Escape cancels; releasing
// the initiating mouse buttons drops. The data object/source remain owned by
// the caller for the duration of this synchronous call.
OleDragResult DoOleDragDrop(IDataObject& data, DWORD allowed_effects,
                            IDropSource* source = nullptr) noexcept;

class OleDropTargetRegistration;
struct OleDropTargetRegistrationResult;
OleDropTargetRegistrationResult RegisterOleDropTarget(
    HWND window, Microsoft::WRL::ComPtr<IDropTarget> target) noexcept;

class OleDropTargetRegistration final {
public:
    OleDropTargetRegistration() noexcept = default;
    ~OleDropTargetRegistration();
    OleDropTargetRegistration(const OleDropTargetRegistration&) = delete;
    OleDropTargetRegistration& operator=(const OleDropTargetRegistration&) = delete;
    OleDropTargetRegistration(OleDropTargetRegistration&& other) noexcept;
    OleDropTargetRegistration& operator=(OleDropTargetRegistration&& other) noexcept;

    OleDragResult Revoke() noexcept;
    bool IsRegistered() const noexcept { return registered_; }
    HWND GetWindow() const noexcept { return window_; }
    DWORD GetThreadId() const noexcept { return thread_id_; }

private:
    friend OleDropTargetRegistrationResult RegisterOleDropTarget(
        HWND window, Microsoft::WRL::ComPtr<IDropTarget> target) noexcept;
    OleDropTargetRegistration(HWND window, Microsoft::WRL::ComPtr<IDropTarget> target,
                              DWORD thread_id) noexcept;
    HWND window_ = nullptr;
    Microsoft::WRL::ComPtr<IDropTarget> target_;
    DWORD thread_id_ = 0;
    bool registered_ = false;
};

struct OleDropTargetRegistrationResult {
    OleDropTargetRegistration registration;
    OleDragStatus status = OleDragStatus::com_failure;
    HRESULT error = E_FAIL;
    explicit operator bool() const noexcept { return registration.IsRegistered(); }
};

// Requires ComApartment::ole_sta on the window thread. Registration and revoke
// must occur on that same thread; destruction performs one best-effort revoke.
OleDropTargetRegistrationResult RegisterOleDropTarget(
    HWND window, Microsoft::WRL::ComPtr<IDropTarget> target) noexcept;

}  // namespace mwfl
