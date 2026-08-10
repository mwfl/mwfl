#include <mwtl/async.h>

#include <atomic>
#include <stdexcept>
#include <thread>

int main() {
    using namespace mwtl;
    int revocations = 0;
    {
        UniqueSubscription first([&] { ++revocations; });
        UniqueSubscription second(std::move(first));
        if (first.IsActive() || !second.IsActive() || !second.Revoke() || second.IsActive()) return 1;
        if (!second.Revoke()) return 2;
    }
    if (revocations != 1) return 3;

    int replaced_revocations = 0;
    {
        UniqueSubscription destination([&] { replaced_revocations += 1; });
        UniqueSubscription source([&] { replaced_revocations += 10; });
        destination = std::move(source);
        if (source.IsActive() || !destination.IsActive() || replaced_revocations != 1) return 4;
    }
    if (replaced_revocations != 11) return 5;

    UniqueSubscription throwing([] { throw std::runtime_error("revoke"); });
    if (throwing.Revoke() || !throwing.GetRevokeException() || throwing.Revoke()) return 6;

    AsyncInitialization initialization;
    const auto success = initialization.Begin();
    int deliveries = 0;
    if (!success || AsyncInitialization::Dispatch(success, [&] { ++deliveries; }) !=
                        AsyncDispatchStatus::delivered)
        return 7;
    if (initialization.GetResult().state != AsyncInitializationState::succeeded || deliveries != 1)
        return 8;
    if (AsyncInitialization::Dispatch(success, [&] { ++deliveries; }) !=
        AsyncDispatchStatus::inactive)
        return 9;

    const auto cancelled = initialization.Begin();
    if (!initialization.Cancel() || initialization.GetResult().state !=
                                        AsyncInitializationState::cancelled)
        return 10;
    if (AsyncInitialization::Dispatch(cancelled, [&] { ++deliveries; }) !=
        AsyncDispatchStatus::inactive)
        return 11;

    const auto failed = initialization.Begin();
    if (!initialization.Fail(failed, E_ACCESSDENIED) ||
        initialization.GetResult().error != E_ACCESSDENIED)
        return 12;

    const auto exceptional = initialization.Begin();
    if (AsyncInitialization::Dispatch(exceptional, [] { throw std::runtime_error("callback"); }) !=
        AsyncDispatchStatus::delivered)
        return 13;
    const auto exceptional_result = initialization.GetResult();
    if (exceptional_result.state != AsyncInitializationState::failed ||
        exceptional_result.error != E_UNEXPECTED || !exceptional_result.callback_exception)
        return 14;

    const auto nested = initialization.Begin();
    AsyncDispatchStatus nested_status = AsyncDispatchStatus::delivered;
    if (AsyncInitialization::Dispatch(nested, [&] {
            nested_status = AsyncInitialization::Dispatch(nested, [] {});
        }) != AsyncDispatchStatus::delivered || nested_status != AsyncDispatchStatus::inactive)
        return 15;

    const auto wrong_thread = initialization.Begin();
    std::atomic status{AsyncDispatchStatus::delivered};
    std::thread worker([&] { status.store(AsyncInitialization::Dispatch(wrong_thread, [] {})); });
    worker.join();
    if (status.load() != AsyncDispatchStatus::wrong_thread ||
        initialization.GetResult().state != AsyncInitializationState::pending)
        return 16;
    if (!initialization.Cancel()) return 17;

    AsyncInitializationTicket expired;
    {
        AsyncInitialization temporary;
        expired = temporary.Begin();
    }
    if (AsyncInitialization::Dispatch(expired, [] {}) != AsyncDispatchStatus::inactive) return 18;

    const auto closed = initialization.Begin();
    initialization.Close();
    if (initialization.GetResult().state != AsyncInitializationState::closed ||
        AsyncInitialization::Dispatch(closed, [] {}) != AsyncDispatchStatus::inactive ||
        initialization.Begin())
        return 19;
    return 0;
}
