#include <mwtl/ribbon.h>

#include <array>

class RibbonWorkspace final {
public:
    RibbonWorkspace() {
        commands_.Add(mwtl::Command({201}, L"Save", [this] { ++save_count_; }));
        model_.Add({{101}, {201}, 1, {7}});
        const std::array recent{
            mwtl::RibbonRecentItem{L"one", L"One", L"C:\\one.txt"}};
        model_.SetRecentItems(recent);
    }

    mwtl::RibbonHostResult Create(HWND frame, HINSTANCE module) noexcept {
        auto created = host_.Create(frame, model_, commands_);
        if (!created) return created;
        auto loaded = host_.Load(module, L"APPLICATION_RIBBON");
        if (!loaded) return loaded;
        return host_.InvalidateAll();
    }

    void SetContext(bool visible) noexcept {
        model_.SetContextVisible({7}, visible);
        host_.InvalidateAll();
    }

    void Close() noexcept { host_.Destroy(); }

private:
    int save_count_ = 0;
    mwtl::CommandSet commands_;
    mwtl::RibbonCommandModel model_;
    mwtl::RibbonFrameworkHost host_;
};
