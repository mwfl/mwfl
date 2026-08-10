#pragma once

#include <windows.h>

#include <atlapp.h>

namespace mwfl::detail {

void ReportException(
    const wchar_t* stage, UINT message, const char* description,
    bool show_user) noexcept;
void ReportUnknownException(
    const wchar_t* stage, UINT message, bool show_user) noexcept;

template <typename Owner>
class AcceleratorFilter final : public WTL::CMessageFilter {
public:
    explicit AcceleratorFilter(Owner* owner) noexcept : owner_(owner) {}

    BOOL PreTranslateMessage(MSG* message) override {
        return message != nullptr && owner_->GetHwnd() != nullptr &&
                owner_->GetAccelerators() != nullptr
            ? ::TranslateAcceleratorW(
                  owner_->GetHwnd(), owner_->GetAccelerators(), message)
            : FALSE;
    }

private:
    Owner* owner_;
};

}  // namespace mwfl::detail
