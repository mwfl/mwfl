#include <mwfl/control_batch.h>
#include <mwfl/must.h>

#include <array>
#include <ranges>
#include <string_view>
#include <system_error>

namespace {

struct FakeItems {
    int AddItem(std::wstring_view) noexcept {
        return calls++ == fail_at ? -7 : calls - 1;
    }

    int calls = 0;
    int fail_at = -1;
};

}  // namespace

int main() {
    FakeItems success;
    const mwfl::BatchResult added =
        mwfl::AddItems(success, {L"one", L"two", L"three"});
    if (!added || added.completed != 3 || added.failed_at != -1) return 1;

    FakeItems failure;
    failure.fail_at = 1;
    const mwfl::BatchResult stopped =
        mwfl::AddItems(failure, {L"one", L"two", L"three"});
    if (stopped || stopped.completed != 1 || stopped.failed_at != 1 ||
        stopped.native_result != -7 || failure.calls != 2) {
        return 2;
    }

    const std::array range_items{L"zero", L"one", L"two", L"three"};
    FakeItems ranged;
    auto odd_length = range_items | std::views::filter(
        [](std::wstring_view item) { return item.size() % 2 != 0; });
    const mwfl::BatchResult ranged_result = mwfl::AddItems(ranged, odd_length);
    if (!ranged_result || ranged_result.completed != 3 || ranged.calls != 3)
        return 3;

    const mwfl::BatchResult must_result =
        mwfl::Must(mwfl::AddItems(ranged, range_items), "range population");
    if (must_result.completed != range_items.size()) return 4;

    bool detailed_failure = false;
    try {
        mwfl::Must(stopped, "expected batch failure");
    } catch (const std::runtime_error& error) {
        const std::string_view message = error.what();
        detailed_failure = message.find("expected batch failure") !=
                               std::string_view::npos &&
                           message.find("item 1") != std::string_view::npos &&
                           message.find("native result -7") !=
                               std::string_view::npos;
    }
    if (!detailed_failure) return 5;

    bool win32_failure = false;
    try {
        mwfl::Must(false, "expected Win32 failure");
    } catch (const std::system_error& error) {
        win32_failure = error.code().value() == ERROR_GEN_FAILURE &&
                        std::string_view(error.what()).find(
                            "expected Win32 failure") != std::string_view::npos;
    }
    if (!win32_failure) return 6;
    return 0;
}
