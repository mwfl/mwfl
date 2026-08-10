#include <mwfl/mwfl.h>

#include <compare>
#include <memory>
#include <span>

static_assert(mwfl::DpiContext::FromDpi(144).GetDpi() == 144);

class Cpp20ConsumerWindow final : public mwfl::Window<Cpp20ConsumerWindow> {
public:
    void BuildUI() {}
};

static_assert(mwfl::MainWindow<Cpp20ConsumerWindow>);

class InjectedWindow final : public mwfl::Window<InjectedWindow> {
public:
    explicit InjectedWindow(std::shared_ptr<int> service)
        : service_(std::move(service)) {}
    void BuildUI() { *service_ += 1; }

private:
    std::shared_ptr<int> service_;
};

static_assert(mwfl::MainWindow<InjectedWindow>);
static_assert(!std::default_initializable<InjectedWindow>);

int CompileInjectedStartup(HINSTANCE instance, int show) {
    return mwfl::RunApplication<InjectedWindow>(
        instance, show, {}, {}, std::make_shared<int>(0));
}
static_assert((mwfl::Dip{1.0f} <=> mwfl::Dip{2.0f}) < 0);
static_assert(std::span<const HANDLE>{}.empty());
