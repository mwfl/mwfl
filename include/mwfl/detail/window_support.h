#pragma once

#include <windows.h>

#include <mwfl/message_pump.h>

namespace mwfl::detail {

inline constexpr wchar_t kSystemMessageFontProperty[] =
    L"mwfl.system-message-font.v1";

class SystemMessageFont final {
public:
    ~SystemMessageFont() noexcept;
    SystemMessageFont() noexcept = default;
    SystemMessageFont(const SystemMessageFont&) = delete;
    SystemMessageFont& operator=(const SystemMessageFont&) = delete;

    void Configure(bool enabled) noexcept { enabled_ = enabled; }
    void Apply(HWND window, UINT dpi) noexcept;
    void Detach(HWND window) noexcept;

private:
    HFONT font_ = nullptr;
    bool enabled_ = true;
};

void ReportException(
    const wchar_t* stage, UINT message, const char* description,
    bool show_user) noexcept;
void ReportUnknownException(
    const wchar_t* stage, UINT message, bool show_user) noexcept;

template <typename Owner>
class AcceleratorFilter final : public MessageFilter {
public:
    explicit AcceleratorFilter(Owner* owner) noexcept : owner_(owner) {}

    bool PreTranslateMessage(MSG& message) override {
        return owner_->GetHwnd() != nullptr &&
            owner_->GetAccelerators() != nullptr &&
            ::TranslateAcceleratorW(
                owner_->GetHwnd(), owner_->GetAccelerators(), &message) != 0;
    }

private:
    Owner* owner_;
};

}  // namespace mwfl::detail
