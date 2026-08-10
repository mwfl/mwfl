#pragma once

#include <functional>
#include <string>
#include <utility>

#include <mwfl/controls.h>

namespace mwfl {

struct ValidationResult {
    bool accepted = true;
    std::wstring message;

    static ValidationResult Accept() { return {}; }
    static ValidationResult Reject(std::wstring message) {
        return {false, std::move(message)};
    }
};

class ChangeGate final {
public:
    ChangeGate() noexcept = default;
    ChangeGate(const ChangeGate&) = delete;
    ChangeGate& operator=(const ChangeGate&) = delete;
    ChangeGate(ChangeGate&&) = delete;
    ChangeGate& operator=(ChangeGate&&) = delete;

    class Token final {
    public:
        explicit Token(ChangeGate& owner) noexcept : owner_(&owner) { ++owner_->depth_; }
        ~Token() noexcept { Release(); }
        Token(const Token&) = delete;
        Token& operator=(const Token&) = delete;
        Token(Token&& other) noexcept : owner_(std::exchange(other.owner_, nullptr)) {}
        Token& operator=(Token&& other) noexcept {
            if (this != &other) { Release(); owner_ = std::exchange(other.owner_, nullptr); }
            return *this;
        }
    private:
        void Release() noexcept { if (owner_ != nullptr) { --owner_->depth_; owner_ = nullptr; } }
        ChangeGate* owner_ = nullptr;
    };

    Token Suppress() noexcept { return Token(*this); }
    bool IsSuppressed() const noexcept { return depth_ != 0; }

private:
    unsigned depth_ = 0;
};

template <typename Control, typename Value>
class ValueBinding final {
public:
    using Reader = std::function<Value(const Control&)>;
    using Writer = std::function<bool(Control&, const Value&)>;
    using Validator = std::function<ValidationResult(const Value&)>;

    ValueBinding(Control& control, Value& model, Reader reader, Writer writer,
                 Validator validator = {})
        : control_(&control), model_(&model), reader_(std::move(reader)),
          writer_(std::move(writer)), validator_(std::move(validator)) {}

    ValueBinding(const ValueBinding&) = delete;
    ValueBinding& operator=(const ValueBinding&) = delete;

    bool Push() {
        auto suppression = gate_.Suppress();
        return writer_(*control_, *model_);
    }

    ValidationResult Pull() {
        if (gate_.IsSuppressed()) return ValidationResult::Accept();
        Value candidate = reader_(*control_);
        ValidationResult result = validator_
            ? validator_(candidate) : ValidationResult::Accept();
        if (result.accepted) *model_ = std::move(candidate);
        return result;
    }

    bool IsSuppressed() const noexcept { return gate_.IsSuppressed(); }
    ChangeGate::Token Suppress() noexcept { return gate_.Suppress(); }

private:
    Control* control_;  // Non-owning; control must outlive the binding.
    Value* model_;      // Non-owning; model must outlive the binding.
    Reader reader_;
    Writer writer_;
    Validator validator_;
    ChangeGate gate_;
};

inline auto BindText(
    NativeControl& control, std::wstring& model,
    ValueBinding<NativeControl, std::wstring>::Validator validator = {}) {
    return ValueBinding<NativeControl, std::wstring>(
        control, model,
        [](const NativeControl& value) { return value.GetText(); },
        [](NativeControl& value, const std::wstring& text) {
            return value.SetText(text);
        },
        std::move(validator));
}

inline auto BindChecked(CheckBox& control, bool& model) {
    return ValueBinding<CheckBox, bool>(
        control, model,
        [](const CheckBox& value) { return value.IsChecked(); },
        [](CheckBox& value, const bool& checked) {
            value.SetChecked(checked);
            return true;
        });
}

inline auto BindSelection(ComboBox& control, int& model) {
    return ValueBinding<ComboBox, int>(
        control, model,
        [](const ComboBox& value) { return value.GetSelection(); },
        [](ComboBox& value, const int& selected) {
            return value.SetSelection(selected);
        });
}

}  // namespace mwfl
