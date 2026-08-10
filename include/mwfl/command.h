#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <mwfl/events.h>

namespace mwfl {

struct CommandShortcut {
    BYTE modifiers = FVIRTKEY;
    WORD key = 0;
};

class Command final {
public:
    using Handler = std::function<void()>;

    Command(ControlId id, std::wstring text, Handler handler = {});
    ControlId GetId() const noexcept { return id_; }
    std::wstring_view GetText() const noexcept { return text_; }
    bool IsEnabled() const noexcept { return enabled_; }
    bool IsChecked() const noexcept { return checked_; }
    bool IsVisible() const noexcept { return visible_; }
    const std::optional<int>& GetImageIndex() const noexcept {
        return image_index_;
    }
    const std::optional<CommandShortcut>& GetShortcut() const noexcept {
        return shortcut_;
    }
    Command& SetText(std::wstring value) { text_ = std::move(value); return *this; }
    Command& SetEnabled(bool value) noexcept { enabled_ = value; return *this; }
    Command& SetChecked(bool value) noexcept { checked_ = value; return *this; }
    Command& SetVisible(bool value) noexcept { visible_ = value; return *this; }
    Command& SetImageIndex(int value) noexcept {
        image_index_ = value;
        return *this;
    }
    Command& ClearImageIndex() noexcept { image_index_.reset(); return *this; }
    Command& SetHandler(Handler value) { handler_ = std::move(value); return *this; }
    Command& SetShortcut(CommandShortcut value) noexcept {
        shortcut_ = value;
        return *this;
    }
    Command& ClearShortcut() noexcept { shortcut_.reset(); return *this; }
    void Invoke() const;

private:
    ControlId id_{};
    std::wstring text_;
    Handler handler_;
    bool enabled_ = true;
    bool checked_ = false;
    bool visible_ = true;
    std::optional<int> image_index_;
    std::optional<CommandShortcut> shortcut_;
};

class CommandSet final {
public:
    CommandSet& Add(Command command);
    Command* Find(ControlId id) noexcept;
    const Command* Find(ControlId id) const noexcept;
    EventResult Dispatch(const CommandEvent& event) const;
    std::size_t GetCount() const noexcept { return commands_.size(); }
    std::span<const Command> GetCommands() const noexcept { return commands_; }

private:
    std::vector<Command> commands_;
};

}  // namespace mwfl
