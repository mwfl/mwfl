#include <mwfl/mdi.h>

class LegacyWorkspace final {
public:
    bool Create(HWND frame) noexcept {
        if (!model_.Add({{1}, L"One"}) || !model_.Add({{2}, L"Two", true}))
            return false;
        return host_.Create(frame, model_) && host_.CreateChild({1}) &&
               host_.CreateChild({2});
    }

    mwfl::MdiModelResult SaveActive() noexcept {
        return mwfl::RouteMdiActiveChild(model_, [this](mwfl::MdiChildId id) {
            last_saved_ = id;
            model_.SetDirty(id, false);
        });
    }

    bool CloseAllAfterDecisions() noexcept {
        while (const auto active = model_.GetActiveId())
            if (!host_.Close(*active, true)) return false;
        return true;
    }

private:
    mwfl::MdiChildId last_saved_{};
    mwfl::MdiWorkspaceModel model_;
    mwfl::MdiHost host_;
};
