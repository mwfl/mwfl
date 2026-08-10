#include <mwfl/ole_drag_drop.h>

#include <objbase.h>
#include <wrl/implements.h>

#include <utility>

namespace mwfl {
namespace {

using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;

bool IsOleSta() noexcept {
    APTTYPE type{};
    APTTYPEQUALIFIER qualifier{};
    return SUCCEEDED(::CoGetApartmentType(&type, &qualifier)) &&
           (type == APTTYPE_STA || type == APTTYPE_MAINSTA);
}

OleDragStatus MapError(HRESULT error) noexcept {
    if (error == DRAGDROP_S_CANCEL) return OleDragStatus::cancelled;
    if (error == E_INVALIDARG || error == E_POINTER) return OleDragStatus::invalid_argument;
    if (error == RPC_E_WRONG_THREAD) return OleDragStatus::wrong_thread;
    if (error == CO_E_NOTINITIALIZED || error == RPC_E_CHANGED_MODE)
        return OleDragStatus::wrong_apartment;
    if (error == DRAGDROP_E_ALREADYREGISTERED) return OleDragStatus::already_registered;
    if (error == DRAGDROP_E_NOTREGISTERED) return OleDragStatus::not_registered;
    if (error == HRESULT_FROM_WIN32(ERROR_INVALID_WINDOW_HANDLE))
        return OleDragStatus::window_destroyed;
    return OleDragStatus::com_failure;
}

DWORD MaskEffects(DWORD effects) noexcept {
    return effects & (DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK | DROPEFFECT_SCROLL);
}

DWORD ResolveEffect(DWORD allowed, DWORD keys, DWORD preferred) noexcept {
    const DWORD requested = preferred & (DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK);
    return requested == DROPEFFECT_NONE ? DROPEFFECT_NONE
                                        : NegotiateDropEffect(allowed, keys, requested);
}

class DropSource final : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IDropSource> {
public:
    DropSource() noexcept : thread_id_(::GetCurrentThreadId()) {}

    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape_pressed,
                                                 DWORD key_state) noexcept override {
        if (::GetCurrentThreadId() != thread_id_) return RPC_E_WRONG_THREAD;
        if (escape_pressed) return DRAGDROP_S_CANCEL;
        if ((key_state & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)) == 0)
            return DRAGDROP_S_DROP;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) noexcept override {
        return ::GetCurrentThreadId() == thread_id_ ? DRAGDROP_S_USEDEFAULTCURSORS
                                                    : RPC_E_WRONG_THREAD;
    }

private:
    DWORD thread_id_;
};

class DropTarget final : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IDropTarget> {
public:
    explicit DropTarget(OleDropTargetCallbacks callbacks)
        : callbacks_(std::move(callbacks)), thread_id_(::GetCurrentThreadId()) {}

    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data, DWORD keys, POINTL point,
                                        DWORD* effect) noexcept override {
        if (::GetCurrentThreadId() != thread_id_) return WrongThread(effect);
        if (!data || !effect) return E_INVALIDARG;
        inside_ = true;
        const DWORD allowed = MaskEffects(*effect);
        try {
            const DWORD preferred = callbacks_.enter
                ? callbacks_.enter(*data, keys, point, allowed) : DROPEFFECT_NONE;
            *effect = ResolveEffect(allowed, keys, preferred);
            return S_OK;
        } catch (...) {
            inside_ = false;
            *effect = DROPEFFECT_NONE;
            return E_UNEXPECTED;
        }
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD keys, POINTL point,
                                       DWORD* effect) noexcept override {
        if (::GetCurrentThreadId() != thread_id_) return WrongThread(effect);
        if (!effect) return E_INVALIDARG;
        if (!inside_) { *effect = DROPEFFECT_NONE; return E_UNEXPECTED; }
        const DWORD allowed = MaskEffects(*effect);
        try {
            const DWORD preferred = callbacks_.over
                ? callbacks_.over(keys, point, allowed) : DROPEFFECT_NONE;
            *effect = ResolveEffect(allowed, keys, preferred);
            return S_OK;
        } catch (...) {
            *effect = DROPEFFECT_NONE;
            return E_UNEXPECTED;
        }
    }
    HRESULT STDMETHODCALLTYPE DragLeave() noexcept override {
        if (::GetCurrentThreadId() != thread_id_) return RPC_E_WRONG_THREAD;
        if (!inside_) return S_FALSE;
        inside_ = false;
        try {
            if (callbacks_.leave) callbacks_.leave();
            return S_OK;
        } catch (...) {
            return E_UNEXPECTED;
        }
    }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* data, DWORD keys, POINTL point,
                                   DWORD* effect) noexcept override {
        if (::GetCurrentThreadId() != thread_id_) return WrongThread(effect);
        if (!data || !effect) return E_INVALIDARG;
        if (!inside_) { *effect = DROPEFFECT_NONE; return E_UNEXPECTED; }
        inside_ = false;
        const DWORD allowed = MaskEffects(*effect);
        try {
            const DWORD preferred = callbacks_.drop
                ? callbacks_.drop(*data, keys, point, allowed) : DROPEFFECT_NONE;
            *effect = ResolveEffect(allowed, keys, preferred);
            return S_OK;
        } catch (...) {
            *effect = DROPEFFECT_NONE;
            return E_UNEXPECTED;
        }
    }

private:
    static HRESULT WrongThread(DWORD* effect) noexcept {
        if (effect) *effect = DROPEFFECT_NONE;
        return RPC_E_WRONG_THREAD;
    }
    OleDropTargetCallbacks callbacks_;
    DWORD thread_id_;
    bool inside_ = false;
};

}  // namespace

ComPtr<IDropSource> CreateOleDropSource() { return Make<DropSource>(); }

ComPtr<IDropTarget> CreateOleDropTarget(OleDropTargetCallbacks callbacks) {
    return Make<DropTarget>(std::move(callbacks));
}

OleDragResult DoOleDragDrop(IDataObject& data, DWORD allowed_effects,
                            IDropSource* source) noexcept {
    OleDragResult result;
    allowed_effects = MaskEffects(allowed_effects) & ~DROPEFFECT_SCROLL;
    if (allowed_effects == 0) {
        result.status = OleDragStatus::invalid_argument;
        result.error = E_INVALIDARG;
        return result;
    }
    if (!IsOleSta()) {
        result.status = OleDragStatus::wrong_apartment;
        result.error = CO_E_NOTINITIALIZED;
        return result;
    }
    ComPtr<IDropSource> owned_source;
    if (!source) {
        owned_source = CreateOleDropSource();
        source = owned_source.Get();
    }
    if (!source) {
        result.status = OleDragStatus::com_failure;
        result.error = E_OUTOFMEMORY;
        return result;
    }
    result.error = ::DoDragDrop(&data, source, allowed_effects, &result.effect);
    if (result.error == DRAGDROP_S_DROP) {
        result.status = OleDragStatus::success;
        result.error = S_OK;
    } else {
        result.status = MapError(result.error);
    }
    return result;
}

OleDropTargetRegistration::OleDropTargetRegistration(
    HWND window, ComPtr<IDropTarget> target, DWORD thread_id) noexcept
    : window_(window), target_(std::move(target)), thread_id_(thread_id), registered_(true) {}

OleDropTargetRegistration::~OleDropTargetRegistration() {
    if (registered_ && ::GetCurrentThreadId() == thread_id_) Revoke();
}

OleDropTargetRegistration::OleDropTargetRegistration(
    OleDropTargetRegistration&& other) noexcept
    : window_(std::exchange(other.window_, nullptr)), target_(std::move(other.target_)),
      thread_id_(std::exchange(other.thread_id_, 0)),
      registered_(std::exchange(other.registered_, false)) {}

OleDropTargetRegistration& OleDropTargetRegistration::operator=(
    OleDropTargetRegistration&& other) noexcept {
    if (this != &other) {
        if (registered_ && ::GetCurrentThreadId() == thread_id_) Revoke();
        window_ = std::exchange(other.window_, nullptr);
        target_ = std::move(other.target_);
        thread_id_ = std::exchange(other.thread_id_, 0);
        registered_ = std::exchange(other.registered_, false);
    }
    return *this;
}

OleDragResult OleDropTargetRegistration::Revoke() noexcept {
    OleDragResult result;
    if (!registered_) {
        result.status = OleDragStatus::not_registered;
        result.error = DRAGDROP_E_NOTREGISTERED;
        return result;
    }
    if (::GetCurrentThreadId() != thread_id_) {
        result.status = OleDragStatus::wrong_thread;
        result.error = RPC_E_WRONG_THREAD;
        return result;
    }
    registered_ = false;
    if (!::IsWindow(window_)) {
        target_.Reset();
        window_ = nullptr;
        thread_id_ = 0;
        result.status = OleDragStatus::window_destroyed;
        result.error = HRESULT_FROM_WIN32(ERROR_INVALID_WINDOW_HANDLE);
        return result;
    }
    result.error = ::RevokeDragDrop(window_);
    target_.Reset();
    if (SUCCEEDED(result.error)) {
        result.status = OleDragStatus::success;
        result.error = S_OK;
    } else {
        result.status = MapError(result.error);
    }
    window_ = nullptr;
    thread_id_ = 0;
    return result;
}

OleDropTargetRegistrationResult RegisterOleDropTarget(
    HWND window, ComPtr<IDropTarget> target) noexcept {
    OleDropTargetRegistrationResult result;
    if (!window || !target || !::IsWindow(window)) {
        result.status = OleDragStatus::invalid_argument;
        result.error = E_INVALIDARG;
        return result;
    }
    if (!IsOleSta()) {
        result.status = OleDragStatus::wrong_apartment;
        result.error = CO_E_NOTINITIALIZED;
        return result;
    }
    const DWORD thread_id = ::GetWindowThreadProcessId(window, nullptr);
    if (thread_id != ::GetCurrentThreadId()) {
        result.status = OleDragStatus::wrong_thread;
        result.error = RPC_E_WRONG_THREAD;
        return result;
    }
    result.error = ::RegisterDragDrop(window, target.Get());
    if (FAILED(result.error)) {
        result.status = MapError(result.error);
        return result;
    }
    result.registration = OleDropTargetRegistration(window, std::move(target), thread_id);
    result.status = OleDragStatus::success;
    result.error = S_OK;
    return result;
}

}  // namespace mwfl
