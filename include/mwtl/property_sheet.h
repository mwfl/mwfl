#pragma once

#include <windows.h>

#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <mwtl/dpi.h>
#include <mwtl/layout.h>

namespace mwtl {

struct PropertyPageId {
    std::uint64_t value = 0;
    constexpr auto operator<=>(const PropertyPageId&) const noexcept = default;
};

struct PropertyPageEntry {
    PropertyPageId id{};
    std::wstring title;
    bool dirty = false;
};

enum class PropertyPageApplyOutcome {
    applied,
    validation_failed,
    failed,
};

class PropertySheetModel final {
public:
    bool Add(PropertyPageEntry page);
    bool Remove(PropertyPageId id) noexcept;
    bool Select(PropertyPageId id) noexcept;
    bool SetDirty(PropertyPageId id, bool dirty = true) noexcept;
    bool Apply(PropertyPageId id, PropertyPageApplyOutcome outcome) noexcept;
    void Reset() noexcept;

    std::span<const PropertyPageEntry> GetPages() const noexcept;
    std::optional<PropertyPageId> GetSelectedId() const noexcept;
    const PropertyPageEntry* Find(PropertyPageId id) const noexcept;
    bool AnyDirty() const noexcept;

private:
    PropertyPageEntry* FindMutable(PropertyPageId id) noexcept;

    std::vector<PropertyPageEntry> pages_;
    std::optional<PropertyPageId> selected_;
};

enum class PropertyPageValidation {
    valid,
    invalid,
};

struct PropertyPageCallbacks {
    std::function<bool(HWND)> initialize;
    std::function<bool(HWND, WORD, WORD)> command;
    std::function<PropertyPageValidation(HWND)> validate;
    std::function<bool(HWND)> apply;
    std::function<void(HWND)> reset;
};

struct PropertyPageOptions {
    PropertyPageId id{};
    std::wstring title;
    PropertyPageCallbacks callbacks;
};

class PropertyPage final {
public:
    explicit PropertyPage(PropertyPageOptions options);
    ~PropertyPage() noexcept;

    PropertyPage(const PropertyPage&) = delete;
    PropertyPage& operator=(const PropertyPage&) = delete;
    PropertyPage(PropertyPage&&) noexcept;
    PropertyPage& operator=(PropertyPage&&) noexcept;

    PropertyPageId GetId() const noexcept;
    std::wstring_view GetTitle() const noexcept;
    HWND GetHwnd() const noexcept;
    bool IsDirty() const noexcept;
    bool SetDirty(bool dirty = true) noexcept;
    bool SetLayout(LayoutNode root);
    void ClearLayout() noexcept;

private:
    struct State;
    std::shared_ptr<State> state_;
    friend class PropertySheetDialog;
};

enum class PropertySheetStatus {
    accepted,
    cancelled,
    failed,
};

struct PropertySheetResult {
    PropertySheetStatus status = PropertySheetStatus::failed;
    INT_PTR native_result = -1;
    DWORD error = ERROR_SUCCESS;
    std::exception_ptr callback_exception{};

    explicit operator bool() const noexcept { return status == PropertySheetStatus::accepted; }
};

struct PropertySheetOptions {
    HWND owner = nullptr;  // Borrowed and disabled only by the native modal call.
    std::wstring title;
    std::optional<PropertyPageId> start_page;
    bool resizable = true;
    bool show_apply = true;
};

// Move-only owner for a modeless native property-sheet HWND. Page C++ wrappers
// may go out of scope after creation, but captures stored in their callbacks
// must remain valid until this object closes. Operations belong to the creating
// UI thread. The destructor closes the sheet deterministically.
class PropertySheetDialog final {
public:
    PropertySheetDialog() noexcept = default;
    ~PropertySheetDialog() noexcept;

    PropertySheetDialog(const PropertySheetDialog&) = delete;
    PropertySheetDialog& operator=(const PropertySheetDialog&) = delete;
    PropertySheetDialog(PropertySheetDialog&& other) noexcept;
    PropertySheetDialog& operator=(PropertySheetDialog&& other) noexcept;

    static PropertySheetResult ShowModal(const PropertySheetOptions& options,
                                         std::span<PropertyPage> pages) noexcept;

    bool CreateModeless(const PropertySheetOptions& options,
                        std::span<PropertyPage> pages) noexcept;
    HWND GetHwnd() const noexcept;
    bool IsWindow() const noexcept;
    bool IsOwnerThread() const noexcept;
    PropertySheetResult GetResult() const noexcept;
    void Close() noexcept;

private:
    struct State;
    static INT_PTR CALLBACK PageProcedure(HWND page, UINT message, WPARAM wparam,
                                          LPARAM lparam) noexcept;
    static int CALLBACK SheetCallback(HWND sheet, UINT message, LPARAM lparam) noexcept;
    static LRESULT CALLBACK SheetSubclass(HWND sheet, UINT message, WPARAM wparam, LPARAM lparam,
                                          UINT_PTR subclass_id, DWORD_PTR reference) noexcept;
    std::shared_ptr<State> state_;
};

}  // namespace mwtl
