#include <mwfl/mdi.h>

#include <algorithm>
#include <utility>

namespace mwfl {
namespace {

MdiModelResult Result(MdiModelStatus status,
                      std::optional<MdiChildId> child = {}) noexcept {
    return {status, child};
}

bool ValidTitle(const std::wstring& title) noexcept {
    return !title.empty() && title.size() <= 32768;
}

}  // namespace

MdiChildEntry* MdiWorkspaceModel::Find(MdiChildId child) noexcept {
    const auto found = std::find_if(children_.begin(), children_.end(),
        [child](const MdiChildEntry& value) { return value.id == child; });
    return found == children_.end() ? nullptr : &*found;
}

const MdiChildEntry* MdiWorkspaceModel::Find(MdiChildId child) const noexcept {
    const auto found = std::find_if(children_.begin(), children_.end(),
        [child](const MdiChildEntry& value) { return value.id == child; });
    return found == children_.end() ? nullptr : &*found;
}

MdiModelResult MdiWorkspaceModel::Add(MdiChildEntry child) noexcept {
    if (!child.id || !ValidTitle(child.title))
        return Result(MdiModelStatus::invalid_argument);
    if (Find(child.id)) return Result(MdiModelStatus::duplicate_id, child.id);
    if (children_.size() >= MaximumChildren)
        return Result(MdiModelStatus::limit_exceeded, child.id);
    try { children_.push_back(std::move(child)); }
    catch (...) {
        return {MdiModelStatus::limit_exceeded, {}, std::current_exception()};
    }
    active_ = children_.back().id;
    ++revision_;
    return Result(MdiModelStatus::success, active_);
}

MdiModelResult MdiWorkspaceModel::Remove(MdiChildId child, bool force) noexcept {
    const auto found = std::find_if(children_.begin(), children_.end(),
        [child](const MdiChildEntry& value) { return value.id == child; });
    if (found == children_.end()) return Result(MdiModelStatus::not_found, child);
    if (!force && !found->closable)
        return Result(MdiModelStatus::not_closable, child);
    const bool was_active = active_ == child;
    const auto index = static_cast<std::size_t>(found - children_.begin());
    children_.erase(found);
    if (was_active) {
        if (children_.empty()) active_.reset();
        else active_ = children_[(std::min)(index, children_.size() - 1)].id;
    }
    ++revision_;
    return Result(MdiModelStatus::success, child);
}

MdiModelResult MdiWorkspaceModel::Activate(MdiChildId child) noexcept {
    if (!Find(child)) return Result(MdiModelStatus::not_found, child);
    if (active_ != child) { active_ = child; ++revision_; }
    return Result(MdiModelStatus::success, child);
}

MdiModelResult MdiWorkspaceModel::Move(MdiChildId child,
                                       std::size_t index) noexcept {
    if (index >= children_.size()) return Result(MdiModelStatus::invalid_argument, child);
    const auto found = std::find_if(children_.begin(), children_.end(),
        [child](const MdiChildEntry& value) { return value.id == child; });
    if (found == children_.end()) return Result(MdiModelStatus::not_found, child);
    const auto old_index = static_cast<std::size_t>(found - children_.begin());
    if (old_index == index) return Result(MdiModelStatus::success, child);
    MdiChildEntry moved = std::move(*found);
    children_.erase(found);
    children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(index),
                     std::move(moved));
    ++revision_;
    return Result(MdiModelStatus::success, child);
}

MdiModelResult MdiWorkspaceModel::TransferTo(
    MdiChildId child, MdiWorkspaceModel& destination, std::size_t index) noexcept {
    if (&destination == this || index > destination.children_.size())
        return Result(MdiModelStatus::invalid_argument, child);
    const MdiChildEntry* source = Find(child);
    if (!source) return Result(MdiModelStatus::not_found, child);
    if (destination.Find(child)) return Result(MdiModelStatus::duplicate_id, child);
    if (destination.children_.size() >= MaximumChildren)
        return Result(MdiModelStatus::limit_exceeded, child);
    try {
        destination.children_.insert(
            destination.children_.begin() + static_cast<std::ptrdiff_t>(index), *source);
    } catch (...) {
        return {MdiModelStatus::limit_exceeded, child, std::current_exception()};
    }
    const bool was_active = active_ == child;
    const auto source_index = static_cast<std::size_t>(source - children_.data());
    children_.erase(children_.begin() + static_cast<std::ptrdiff_t>(source_index));
    if (was_active) {
        if (children_.empty()) active_.reset();
        else active_ = children_[(std::min)(source_index, children_.size() - 1)].id;
    }
    destination.active_ = child;
    ++revision_;
    ++destination.revision_;
    return Result(MdiModelStatus::success, child);
}

MdiModelResult MdiWorkspaceModel::Rename(MdiChildId child,
                                         std::wstring title) noexcept {
    if (!ValidTitle(title)) return Result(MdiModelStatus::invalid_argument, child);
    MdiChildEntry* found = Find(child);
    if (!found) return Result(MdiModelStatus::not_found, child);
    try { found->title = std::move(title); }
    catch (...) { return {MdiModelStatus::limit_exceeded, child, std::current_exception()}; }
    ++revision_;
    return Result(MdiModelStatus::success, child);
}

MdiModelResult MdiWorkspaceModel::SetDirty(MdiChildId child, bool dirty) noexcept {
    MdiChildEntry* found = Find(child);
    if (!found) return Result(MdiModelStatus::not_found, child);
    if (found->dirty != dirty) { found->dirty = dirty; ++revision_; }
    return Result(MdiModelStatus::success, child);
}

MdiModelResult MdiWorkspaceModel::SetClosable(MdiChildId child,
                                              bool closable) noexcept {
    MdiChildEntry* found = Find(child);
    if (!found) return Result(MdiModelStatus::not_found, child);
    if (found->closable != closable) { found->closable = closable; ++revision_; }
    return Result(MdiModelStatus::success, child);
}

MdiModelResult RouteMdiActiveChild(
    const MdiWorkspaceModel& workspace,
    const MdiActiveChildHandler& handler) noexcept {
    if (!handler) return Result(MdiModelStatus::invalid_argument);
    const auto active = workspace.GetActiveId();
    if (!active) return Result(MdiModelStatus::not_found);
    try { handler(*active); }
    catch (...) {
        return {MdiModelStatus::callback_failed, active, std::current_exception()};
    }
    if (workspace.GetActiveId() != active)
        return Result(MdiModelStatus::active_child_changed, active);
    return Result(MdiModelStatus::success, active);
}

struct MdiHost::ChildState {
    MdiHost* host = nullptr;
    MdiChildId id{};
    HWND window = nullptr;
    MdiChildMessageHandler message;
};

namespace {

constexpr wchar_t MdiChildClassName[] = L"mwfl.MdiHost.Child";

bool RegisterMdiChildClass() noexcept {
    WNDCLASSW window_class{};
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = MdiHost::ChildProcedure;
    window_class.cbWndExtra = sizeof(void*);
    window_class.hInstance = ::GetModuleHandleW(nullptr);
    window_class.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window_class.lpszClassName = MdiChildClassName;
    return ::RegisterClassW(&window_class) != 0 ||
           ::GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

}  // namespace

MdiHost::MdiHost() noexcept = default;
MdiHost::~MdiHost() noexcept { Destroy(); }

MdiHostResult MdiHost::CheckThread() const noexcept {
    if (!client_) return {MdiHostStatus::invalid_state};
    if (::GetCurrentThreadId() != thread_id_)
        return {MdiHostStatus::wrong_thread, {}, nullptr, ERROR_INVALID_THREAD_ID};
    return {MdiHostStatus::success};
}

MdiHostResult MdiHost::Create(HWND frame, MdiWorkspaceModel& model,
                              HMENU window_menu,
                              UINT first_child_command) noexcept {
    if (client_ || !frame || !::IsWindow(frame) || first_child_command == 0)
        return {MdiHostStatus::invalid_argument, {}, nullptr, ERROR_INVALID_PARAMETER};
    DWORD process = 0;
    const DWORD thread = ::GetWindowThreadProcessId(frame, &process);
    if (thread != ::GetCurrentThreadId() || process != ::GetCurrentProcessId())
        return {MdiHostStatus::wrong_thread, {}, nullptr, ERROR_INVALID_THREAD_ID};
    if (!RegisterMdiChildClass())
        return {MdiHostStatus::native_failure, {}, nullptr, ::GetLastError()};
    CLIENTCREATESTRUCT create{window_menu, first_child_command};
    HWND client = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"MDICLIENT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL,
        0, 0, 0, 0, frame, nullptr, ::GetModuleHandleW(nullptr), &create);
    if (!client)
        return {MdiHostStatus::native_failure, {}, nullptr, ::GetLastError()};
    frame_ = frame;
    client_ = client;
    thread_id_ = thread;
    model_ = &model;
    return {MdiHostStatus::success, {}, client};
}

HWND MdiHost::GetChild(MdiChildId child) const noexcept {
    const auto found = std::find_if(children_.begin(), children_.end(),
        [child](const auto& value) { return value->id == child; });
    return found == children_.end() ? nullptr : (*found)->window;
}

std::optional<MdiChildId> MdiHost::GetChildId(HWND child) const noexcept {
    const auto found = std::find_if(children_.begin(), children_.end(),
        [child](const auto& value) { return value->window == child; });
    return found == children_.end() ? std::nullopt
                                   : std::optional{(*found)->id};
}

MdiHostResult MdiHost::CreateChild(MdiChildId child,
                                   MdiChildOptions options) noexcept {
    const auto checked = CheckThread();
    if (!checked) return checked;
    const MdiChildEntry* entry = model_->Find(child);
    if (!entry || GetChild(child))
        return {MdiHostStatus::invalid_argument, child, nullptr, ERROR_INVALID_PARAMETER};
    std::unique_ptr<ChildState> state;
    try {
        state = std::make_unique<ChildState>(
            ChildState{this, child, nullptr, std::move(options.message)});
        children_.push_back(std::move(state));
    } catch (...) {
        return {MdiHostStatus::native_failure, child, nullptr, ERROR_OUTOFMEMORY,
                std::current_exception()};
    }
    ChildState* raw = children_.back().get();
    MDICREATESTRUCTW create{};
    create.szClass = MdiChildClassName;
    create.szTitle = entry->title.c_str();
    create.hOwner = ::GetModuleHandleW(nullptr);
    create.style = options.style;
    create.lParam = reinterpret_cast<LPARAM>(raw);
    HWND window = reinterpret_cast<HWND>(::SendMessageW(
        client_, WM_MDICREATE, 0, reinterpret_cast<LPARAM>(&create)));
    if (!window) {
        const DWORD error = ::GetLastError();
        children_.pop_back();
        return {MdiHostStatus::native_failure, child, nullptr, error};
    }
    raw->window = window;
    model_->Activate(child);
    return {MdiHostStatus::success, child, window};
}

MdiHostResult MdiHost::Activate(MdiChildId child) noexcept {
    const auto checked = CheckThread();
    if (!checked) return checked;
    HWND window = GetChild(child);
    if (!window) return {MdiHostStatus::invalid_argument, child};
    const auto activated = model_->Activate(child);
    if (!activated) return {MdiHostStatus::model_failure, child};
    ::SendMessageW(client_, WM_MDIACTIVATE, reinterpret_cast<WPARAM>(window), 0);
    return {MdiHostStatus::success, child, window};
}

MdiHostResult MdiHost::Close(MdiChildId child, bool force) noexcept {
    const auto checked = CheckThread();
    if (!checked) return checked;
    HWND window = GetChild(child);
    const MdiChildEntry* entry = model_->Find(child);
    if (!window || !entry) return {MdiHostStatus::invalid_argument, child};
    if (!force && !entry->closable)
        return {MdiHostStatus::model_failure, child, window, ERROR_ACCESS_DENIED};
    ::SendMessageW(client_, WM_MDIDESTROY, reinterpret_cast<WPARAM>(window), 0);
    if (::IsWindow(window))
        return {MdiHostStatus::native_failure, child, window, ::GetLastError()};
    return {MdiHostStatus::success, child};
}

MdiHostResult MdiHost::Arrange(MdiArrange arrangement) noexcept {
    const auto checked = CheckThread();
    if (!checked) return checked;
    switch (arrangement) {
    case MdiArrange::cascade:
        ::SendMessageW(client_, WM_MDICASCADE, MDITILE_SKIPDISABLED, 0); break;
    case MdiArrange::tile_horizontal:
        ::SendMessageW(client_, WM_MDITILE, MDITILE_HORIZONTAL | MDITILE_SKIPDISABLED, 0); break;
    case MdiArrange::tile_vertical:
        ::SendMessageW(client_, WM_MDITILE, MDITILE_VERTICAL | MDITILE_SKIPDISABLED, 0); break;
    case MdiArrange::arrange_icons:
        ::SendMessageW(client_, WM_MDIICONARRANGE, 0, 0); break;
    }
    return {MdiHostStatus::success};
}

bool MdiHost::Translate(MSG& message) const noexcept {
    return client_ && ::TranslateMDISysAccel(client_, &message) != FALSE;
}

LRESULT MdiHost::FrameDefault(UINT message, WPARAM wparam,
                              LPARAM lparam) const noexcept {
    return ::DefFrameProcW(frame_, client_, message, wparam, lparam);
}

void MdiHost::Resize(RECT bounds) noexcept {
    if (client_ && ::GetCurrentThreadId() == thread_id_)
        ::MoveWindow(client_, bounds.left, bounds.top, bounds.right - bounds.left,
                     bounds.bottom - bounds.top, TRUE);
}

std::exception_ptr MdiHost::TakeCallbackException() noexcept {
    auto result = callback_exception_;
    callback_exception_ = nullptr;
    return result;
}

void MdiHost::ChildDestroyed(ChildState* child) noexcept {
    if (!child) return;
    const auto found = std::find_if(children_.begin(), children_.end(),
        [child](const auto& value) { return value.get() == child; });
    if (found == children_.end()) return;
    model_->Remove(child->id, true);
    children_.erase(found);
}

LRESULT CALLBACK MdiHost::ChildProcedure(HWND window, UINT message,
                                         WPARAM wparam, LPARAM lparam) noexcept {
    ChildState* state = reinterpret_cast<ChildState*>(
        ::GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        const auto* mdi = static_cast<const MDICREATESTRUCTW*>(create->lpCreateParams);
        state = reinterpret_cast<ChildState*>(mdi ? mdi->lParam : 0);
        if (state) {
            state->window = window;
            ::SetWindowLongPtrW(window, GWLP_USERDATA,
                               reinterpret_cast<LONG_PTR>(state));
        }
    }
    if (state && state->message) {
        try {
            if (auto result = state->message(window, message, wparam, lparam))
                return *result;
        } catch (...) {
            state->host->callback_exception_ = std::current_exception();
        }
    }
    const LRESULT result = ::DefMDIChildProcW(window, message, wparam, lparam);
    if (message == WM_NCDESTROY && state) {
        ::SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        state->host->ChildDestroyed(state);
    }
    return result;
}

void MdiHost::Destroy() noexcept {
    if (!client_) return;
    if (::GetCurrentThreadId() != thread_id_) return;
    while (!children_.empty()) {
        HWND child = children_.back()->window;
        if (child && ::IsWindow(child))
            ::SendMessageW(client_, WM_MDIDESTROY, reinterpret_cast<WPARAM>(child), 0);
        else
            children_.pop_back();
    }
    ::DestroyWindow(client_);
    client_ = nullptr;
    frame_ = nullptr;
    thread_id_ = 0;
    model_ = nullptr;
    callback_exception_ = nullptr;
}

}  // namespace mwfl
