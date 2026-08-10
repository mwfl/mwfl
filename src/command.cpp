#include <mwfl/command.h>

#include <algorithm>

namespace mwfl {

Command::Command(ControlId id, std::wstring text, Handler handler)
    : id_(id), text_(std::move(text)), handler_(std::move(handler)) {}

void Command::Invoke() const {
    if (enabled_ && handler_) handler_();
}

CommandSet& CommandSet::Add(Command command) {
    if (Command* existing = Find(command.GetId()); existing != nullptr) {
        *existing = std::move(command);
    } else {
        commands_.push_back(std::move(command));
    }
    return *this;
}

Command* CommandSet::Find(ControlId id) noexcept {
    const auto found = std::find_if(commands_.begin(), commands_.end(),
        [id](const Command& command) { return command.GetId() == id; });
    return found == commands_.end() ? nullptr : &*found;
}

const Command* CommandSet::Find(ControlId id) const noexcept {
    const auto found = std::find_if(commands_.begin(), commands_.end(),
        [id](const Command& command) { return command.GetId() == id; });
    return found == commands_.end() ? nullptr : &*found;
}

EventResult CommandSet::Dispatch(const CommandEvent& event) const {
    const Command* command = Find(event.id);
    if (command == nullptr || !command->IsEnabled()) return EventResult::Propagate();
    command->Invoke();
    return EventResult::Handled();
}

}  // namespace mwfl
