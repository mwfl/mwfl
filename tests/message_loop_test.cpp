// Runtime coverage for mwfl::MessageLoop, mwfl::MessageFilter, and
// WaitAwareMessagePump, plus the process-wide module lifetime behind
// Application. Modes:
//   unit   - loop activation, filter chain semantics, pumps, accelerators
//   order  - a modeless dialog registered after the main window sees keys first
//   twice  - two sequential Application runs in one process succeed
#include <mwfl/mwfl.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, \
                #condition);                                                 \
            return __LINE__;                                                 \
        }                                                                    \
    } while (0)

namespace {

struct CountingFilter final : mwfl::MessageFilter {
    int calls = 0;
    bool consume = false;
    std::function<void(mwfl::MessageLoop&)> on_call;
    mwfl::MessageLoop* loop = nullptr;

    bool PreTranslateMessage(MSG&) override {
        ++calls;
        if (on_call && loop != nullptr) on_call(*loop);
        return consume;
    }
};

int TestActivationNesting() {
    CHECK(mwfl::MessageLoop::Current() == nullptr);
    {
        mwfl::MessageLoop outer;
        outer.Activate();
        CHECK(mwfl::MessageLoop::Current() == &outer);
        outer.Activate();  // Idempotent.
        CHECK(mwfl::MessageLoop::Current() == &outer);
        {
            mwfl::MessageLoop inner;
            inner.Activate();
            CHECK(mwfl::MessageLoop::Current() == &inner);
            outer.Deactivate();  // Not current: documented no-op.
            CHECK(mwfl::MessageLoop::Current() == &inner);
            inner.Deactivate();
            CHECK(mwfl::MessageLoop::Current() == &outer);
            inner.Deactivate();  // Idempotent.
            CHECK(mwfl::MessageLoop::Current() == &outer);
        }
        {
            mwfl::MessageLoop scoped;
            scoped.Activate();
            CHECK(mwfl::MessageLoop::Current() == &scoped);
        }  // Destroying an active loop deactivates it.
        CHECK(mwfl::MessageLoop::Current() == &outer);

        mwfl::MessageLoop* seen_elsewhere = &outer;
        std::thread([&] {
            seen_elsewhere = mwfl::MessageLoop::Current();
            outer.Deactivate();  // Foreign thread: no-op.
        }).join();
        CHECK(seen_elsewhere == nullptr);
        CHECK(mwfl::MessageLoop::Current() == &outer);
    }
    CHECK(mwfl::MessageLoop::Current() == nullptr);
    return 0;
}

int TestFilterChain() {
    mwfl::MessageLoop loop;
    MSG message{};
    CHECK(!loop.AddFilter(nullptr));
    CHECK(!loop.PreTranslate(message));

    CountingFilter first, second, third;
    for (CountingFilter* filter : {&first, &second, &third}) filter->loop = &loop;
    CHECK(loop.AddFilter(&first));
    CHECK(loop.AddFilter(&second));
    CHECK(loop.AddFilter(&third));
    CHECK(!loop.PreTranslate(message));
    CHECK(first.calls == 1 && second.calls == 1 && third.calls == 1);

    // Newest first: a consuming filter registered last stops older ones.
    third.consume = true;
    CHECK(loop.PreTranslate(message));
    CHECK(first.calls == 1 && second.calls == 1 && third.calls == 2);
    third.consume = false;
    second.consume = true;
    CHECK(loop.PreTranslate(message));
    CHECK(first.calls == 1 && second.calls == 2 && third.calls == 3);
    second.consume = false;

    // Re-adding a registered filter keeps a single registration.
    CHECK(loop.AddFilter(&second));
    CHECK(!loop.PreTranslate(message));
    CHECK(first.calls == 2 && second.calls == 3 && third.calls == 4);

    // A filter that unregisters itself must not stop older filters.
    third.on_call = [&](mwfl::MessageLoop& current) { current.RemoveFilter(&third); };
    CHECK(!loop.PreTranslate(message));
    CHECK(first.calls == 3 && second.calls == 4 && third.calls == 5);
    third.on_call = nullptr;
    CHECK(!loop.PreTranslate(message));
    CHECK(third.calls == 5);  // No longer registered.

    // A filter removing an older filter before its turn: the older is skipped.
    second.on_call = [&](mwfl::MessageLoop& current) { current.RemoveFilter(&first); };
    CHECK(!loop.PreTranslate(message));
    CHECK(first.calls == 4 && second.calls == 6);
    second.on_call = nullptr;

    // Filters added during a pass wait for the next message; reallocation
    // during dispatch is safe.
    CHECK(loop.AddFilter(&first));  // Chain is now [second, first].
    std::vector<CountingFilter> extra(64);
    for (CountingFilter& filter : extra) filter.loop = &loop;
    first.on_call = [&](mwfl::MessageLoop& current) {
        for (CountingFilter& filter : extra) current.AddFilter(&filter);
    };
    CHECK(!loop.PreTranslate(message));
    first.on_call = nullptr;
    int extra_calls = 0;
    for (const CountingFilter& filter : extra) extra_calls += filter.calls;
    CHECK(extra_calls == 0);
    CHECK(second.calls == 7 && first.calls == 5);
    CHECK(!loop.PreTranslate(message));
    extra_calls = 0;
    for (const CountingFilter& filter : extra) extra_calls += filter.calls;
    CHECK(extra_calls == 64);

    // A filter removing itself and every older filter ends the pass cleanly.
    extra.back().on_call = [&](mwfl::MessageLoop& current) {
        for (CountingFilter& filter : extra) current.RemoveFilter(&filter);
        current.RemoveFilter(&first);
        current.RemoveFilter(&second);
    };
    CHECK(!loop.PreTranslate(message));
    CHECK(second.calls == 8 && first.calls == 6);
    CHECK(!loop.PreTranslate(message));
    CHECK(second.calls == 8 && first.calls == 6);
    loop.RemoveFilter(&second);  // Removing an unregistered filter is fine.
    return 0;
}

int TestRunReturnsQuitCode() {
    mwfl::MessageLoop loop;
    CountingFilter filter;
    filter.loop = &loop;
    CHECK(loop.AddFilter(&filter));
    CHECK(::PostMessageW(nullptr, WM_APP + 7, 0, 0) != FALSE);
    ::PostQuitMessage(42);
    CHECK(loop.Run() == 42);
    CHECK(filter.calls == 1);  // The thread message went through the chain.
    return 0;
}

int TestWaitAwarePump() {
    mwfl::MessageLoop loop;
    const HANDLE event = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
    CHECK(event != nullptr);
    std::atomic<int> signals{0};
    std::atomic<int> idles{0};
    const HANDLE handles[] = {event};
    mwfl::WaitAwareMessagePump pump({
        .handles = handles,
        .idle_interval = std::chrono::milliseconds{5},
        .on_idle = [&] { ++idles; },
        .on_signal = [&](std::size_t index) {
            if (index == 0 && ++signals == 3) ::PostQuitMessage(7);
        },
    });
    std::thread producer([&] {
        for (int i = 0; i < 3; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
            ::SetEvent(event);
        }
    });
    const int code = pump.Run(loop);
    producer.join();
    ::CloseHandle(event);
    CHECK(code == 7);
    CHECK(signals == 3);
    CHECK(idles > 0);
    return 0;
}

class AcceleratorWindow final : public mwfl::WindowBase {
public:
    static inline int accelerator_hits = 0;
    static inline HACCEL table = nullptr;

    void BuildUI() override {
        SetAccelerators(table);
        CHECK_POST(::PostMessageW(GetHwnd(), WM_KEYDOWN, VK_F5, 0));
        CHECK_POST(::PostMessageW(GetHwnd(), WM_APP + 1, 0, 0));
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.id.value == 1001) ++accelerator_hits;
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& message) override {
        if (message.id != WM_APP + 1) return mwfl::EventResult::Propagate();
        // Unregister, re-register, translate once more, then close.
        SetAccelerators(nullptr);
        SetAccelerators(table);
        CHECK_POST(::PostMessageW(GetHwnd(), WM_KEYDOWN, VK_F5, 0));
        CHECK_POST(::PostMessageW(GetHwnd(), WM_CLOSE, 0, 0));
        return mwfl::EventResult::Handled();
    }

private:
    static void CHECK_POST(BOOL posted) {
        if (posted == FALSE) throw std::runtime_error("PostMessageW failed");
    }
};

int RunAcceleratorWindowOnce() {
    AcceleratorWindow::accelerator_hits = 0;
    mwfl::Application application(::GetModuleHandleW(nullptr));
    CHECK(application.Run<AcceleratorWindow>(SW_HIDE) == 0);
    CHECK(AcceleratorWindow::accelerator_hits == 2);
    CHECK(mwfl::MessageLoop::Current() == nullptr);
    return 0;
}

int TestAcceleratorLifetime() {
    const ACCEL entry{FVIRTKEY, VK_F5, 1001};
    AcceleratorWindow::table = ::CreateAcceleratorTableW(
        const_cast<ACCEL*>(&entry), 1);
    CHECK(AcceleratorWindow::table != nullptr);
    const int result = RunAcceleratorWindowOnce();
    ::DestroyAcceleratorTable(AcceleratorWindow::table);
    return result;
}

// Multiple Application runs in one process remain independent.
int TestSecondApplicationRun() {
    const ACCEL entry{FVIRTKEY, VK_F5, 1001};
    AcceleratorWindow::table = ::CreateAcceleratorTableW(
        const_cast<ACCEL*>(&entry), 1);
    CHECK(AcceleratorWindow::table != nullptr);
    int result = 0;
    for (int run = 0; run < 3 && result == 0; ++run) {
        result = RunAcceleratorWindowOnce();
    }
    ::DestroyAcceleratorTable(AcceleratorWindow::table);
    return result;
}

// Filter order: the modeless dialog registers its filter after the main
// window's accelerator filter and must still see Escape first.
class OrderWindow final : public mwfl::WindowBase {
public:
    static inline int accelerator_hits = 0;
    static inline int dialog_cancel_hits = 0;
    static inline HACCEL table = nullptr;
    static inline std::optional<mwfl::Dialog> dialog;

    void BuildUI() override {
        SetAccelerators(table);
        dialog.emplace(mwfl::DialogOptions{
            .owner = GetHwnd(),
            .title = L"modeless order probe",
            .callbacks = {
                .command = [](HWND, WORD id, WORD) {
                    if (id == IDCANCEL) ++dialog_cancel_hits;
                    return true;  // Consume; keep the dialog alive.
                },
            },
        });
        if (!dialog->CreateModeless()) throw std::runtime_error("CreateModeless failed");
        if (::PostMessageW(dialog->GetHwnd(), WM_KEYDOWN, VK_ESCAPE, 0) == FALSE ||
            ::PostMessageW(GetHwnd(), WM_APP + 1, 0, 0) == FALSE) {
            throw std::runtime_error("PostMessageW failed");
        }
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.id.value == 1001) ++accelerator_hits;
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& message) override {
        if (message.id == WM_APP + 1) {
            dialog->Close();
            ::PostMessageW(GetHwnd(), WM_APP + 2, 0, 0);
            return mwfl::EventResult::Handled();
        }
        if (message.id == WM_APP + 2) {
            dialog.reset();
            ::PostMessageW(GetHwnd(), WM_CLOSE, 0, 0);
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }
};

int TestFilterOrderWithModelessDialog() {
    const ACCEL entry{FVIRTKEY, VK_ESCAPE, 1001};
    OrderWindow::table = ::CreateAcceleratorTableW(const_cast<ACCEL*>(&entry), 1);
    CHECK(OrderWindow::table != nullptr);
    int code = 0;
    {
        mwfl::Application application(::GetModuleHandleW(nullptr));
        code = application.Run<OrderWindow>(SW_HIDE);
    }
    ::DestroyAcceleratorTable(OrderWindow::table);
    CHECK(code == 0);
    CHECK(OrderWindow::dialog_cancel_hits == 1);
    CHECK(OrderWindow::accelerator_hits == 0);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "unit";
    int result = 0;
    if (std::strcmp(mode, "unit") == 0) {
        if ((result = TestActivationNesting()) != 0) return result;
        if ((result = TestFilterChain()) != 0) return result;
        if ((result = TestRunReturnsQuitCode()) != 0) return result;
        if ((result = TestWaitAwarePump()) != 0) return result;
        if ((result = TestAcceleratorLifetime()) != 0) return result;
    } else if (std::strcmp(mode, "order") == 0) {
        if ((result = TestFilterOrderWithModelessDialog()) != 0) return result;
    } else if (std::strcmp(mode, "twice") == 0) {
        if ((result = TestSecondApplicationRun()) != 0) return result;
    } else {
        std::fprintf(stderr, "unknown test mode: %s\n", mode);
        return 20;
    }
    return 0;
}
