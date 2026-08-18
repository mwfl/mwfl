#include <mwfl/deployment.h>
#include <appmodel.h>
#include <mutex>
#include <softpub.h>
#include <wintrust.h>
#include <winver.h>
namespace mwfl {
namespace {
std::mutex recovery_mutex;
RecoveryCallback recovery_callback;
std::stop_source recovery_stop;
bool recovery_active = false;
DWORD CALLBACK RecoveryThunk(void*) noexcept {
    RecoveryCallback callback;
    std::stop_token stop;
    {
        std::scoped_lock lock(recovery_mutex);
        callback = recovery_callback;
        stop = recovery_stop.get_token();
    }
    bool success = false;
    try {
        if (callback) success = static_cast<bool>(callback(stop));
    } catch (...) {
    }
    ApplicationRecoveryFinished(success);
    return 0;
}
std::wstring Quote(std::wstring_view value) {
    if (!value.empty() && value.find_first_of(L" \t\"") == std::wstring_view::npos)
        return std::wstring(value);
    std::wstring result(1, L'"');
    std::size_t slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        if (c == L'"') {
            result.append(slashes * 2 + 1, L'\\');
            result.push_back(L'"');
            slashes = 0;
            continue;
        }
        result.append(slashes, L'\\');
        slashes = 0;
        result.push_back(c);
    }
    result.append(slashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}
Result<std::wstring> Volume(const std::filesystem::path& path) {
    wchar_t volume[MAX_PATH]{};
    auto absolute = std::filesystem::absolute(path);
    if (!GetVolumePathNameW(absolute.c_str(), volume, MAX_PATH)) return NativeError::LastWin32();
    return std::wstring(volume);
}
bool MinimumSpecified(const Version& version) {
    return version.major || version.minor || version.build || version.revision;
}
}  // namespace
Result<PackageIdentity> QueryCurrentPackageIdentity() {
    UINT32 length = 0;
    LONG result = GetCurrentPackageFullName(&length, nullptr);
    if (result == APPMODEL_ERROR_NO_PACKAGE) return PackageIdentity{};
    if (result != ERROR_INSUFFICIENT_BUFFER) return NativeError::FromWin32(result);
    std::wstring name(length, L'\0');
    result = GetCurrentPackageFullName(&length, name.data());
    if (result != ERROR_SUCCESS) return NativeError::FromWin32(result);
    if (!name.empty() && !name.back()) name.pop_back();
    UINT32 family_length = 0;
    PackageFamilyNameFromFullName(name.c_str(), &family_length, nullptr);
    std::wstring family(family_length, L'\0');
    if (PackageFamilyNameFromFullName(name.c_str(), &family_length, family.data()) != ERROR_SUCCESS)
        family.clear();
    else if (!family.empty() && !family.back())
        family.pop_back();
    UINT32 app_length = 0;
    std::wstring app;
    result = GetCurrentApplicationUserModelId(&app_length, nullptr);
    if (result == ERROR_INSUFFICIENT_BUFFER) {
        app.resize(app_length);
        if (GetCurrentApplicationUserModelId(&app_length, app.data()) == ERROR_SUCCESS &&
            !app.empty() && !app.back())
            app.pop_back();
    }
    PackageIdentity identity{true, std::move(name), std::move(family), std::move(app), {}};
    PACKAGE_ID* id = nullptr;
    UINT32 bytes = 0;
    if (PackageIdFromFullName(identity.full_name.c_str(), 0, &bytes, nullptr) ==
        ERROR_INSUFFICIENT_BUFFER) {
        std::vector<std::byte> storage(bytes);
        id = reinterpret_cast<PACKAGE_ID*>(storage.data());
        if (PackageIdFromFullName(identity.full_name.c_str(), 0, &bytes,
                                  reinterpret_cast<BYTE*>(id)) == ERROR_SUCCESS)
            identity.version = {id->version.Major, id->version.Minor, id->version.Build,
                                id->version.Revision};
    }
    return identity;
}
Result<Version> QueryFileVersion(const std::filesystem::path& path) {
    DWORD ignored = 0;
    const DWORD bytes = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (!bytes) return NativeError::LastWin32().WithOperation(L"GetFileVersionInfoSizeW");
    std::vector<std::byte> storage(bytes);
    if (!GetFileVersionInfoW(path.c_str(), 0, bytes, storage.data()))
        return NativeError::LastWin32();
    VS_FIXEDFILEINFO* info = nullptr;
    UINT size = 0;
    if (!VerQueryValueW(storage.data(), L"\\", reinterpret_cast<void**>(&info), &size) || !info)
        return NativeError::FromWin32(ERROR_RESOURCE_DATA_NOT_FOUND);
    return Version{HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
                   HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS)};
}
Result<void> RegisterApplicationRestart(std::wstring_view arguments, DWORD flags) {
    if (arguments.size() >= RESTART_MAX_CMD_LINE)
        return NativeError::FromWin32(ERROR_INVALID_PARAMETER);
    std::wstring copy(arguments);
    const HRESULT result = ::RegisterApplicationRestart(copy.c_str(), flags);
    return FAILED(result) ? Result<void>{NativeError::FromHResult(result)} : Result<void>{};
}
Result<void> UnregisterApplicationRestart() noexcept {
    const HRESULT result = ::UnregisterApplicationRestart();
    return FAILED(result) ? Result<void>{NativeError::FromHResult(result)} : Result<void>{};
}
RestartRegistration::RestartRegistration(std::wstring_view arguments, DWORD flags) {
    active_ = static_cast<bool>(RegisterApplicationRestart(arguments, flags));
}
RestartRegistration::~RestartRegistration() {
    if (active_) (void)UnregisterApplicationRestart();
}
RestartRegistration::RestartRegistration(RestartRegistration&& other) noexcept
    : active_(std::exchange(other.active_, false)) {}
RestartRegistration& RestartRegistration::operator=(RestartRegistration&& other) noexcept {
    if (this != &other) {
        if (active_) (void)UnregisterApplicationRestart();
        active_ = std::exchange(other.active_, false);
    }
    return *this;
}
Result<RecoveryRegistration> RecoveryRegistration::Register(RecoveryCallback callback,
                                                            std::chrono::milliseconds interval) {
    if (!callback || interval < std::chrono::seconds(5) || interval > std::chrono::minutes(5))
        return NativeError::FromWin32(ERROR_INVALID_PARAMETER);
    std::scoped_lock lock(recovery_mutex);
    if (recovery_active) return NativeError::FromWin32(ERROR_ALREADY_EXISTS);
    recovery_callback = std::move(callback);
    recovery_stop = std::stop_source{};
    const HRESULT result = RegisterApplicationRecoveryCallback(
        RecoveryThunk, nullptr, static_cast<DWORD>(interval.count()), 0);
    if (FAILED(result)) {
        recovery_callback = {};
        return NativeError::FromHResult(result);
    }
    recovery_active = true;
    return RecoveryRegistration(true);
}
RecoveryRegistration::~RecoveryRegistration() {
    if (active_) {
        std::scoped_lock lock(recovery_mutex);
        recovery_stop.request_stop();
        UnregisterApplicationRecoveryCallback();
        recovery_callback = {};
        recovery_active = false;
    }
}
RecoveryRegistration::RecoveryRegistration(RecoveryRegistration&& other) noexcept
    : active_(std::exchange(other.active_, false)) {}
RecoveryRegistration& RecoveryRegistration::operator=(RecoveryRegistration&& other) noexcept {
    if (this != &other) {
        if (active_) {
            std::scoped_lock lock(recovery_mutex);
            recovery_stop.request_stop();
            UnregisterApplicationRecoveryCallback();
            recovery_callback = {};
            recovery_active = false;
        }
        active_ = std::exchange(other.active_, false);
    }
    return *this;
}
Result<SignatureVerification> VerifyAuthenticode(const std::filesystem::path& path,
                                                 RevocationPolicy policy) {
    if (!std::filesystem::is_regular_file(path))
        return NativeError::FromWin32(ERROR_FILE_NOT_FOUND);
    WINTRUST_FILE_INFO file{sizeof(file), path.c_str(), nullptr, nullptr};
    WINTRUST_DATA data{sizeof(data)};
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks =
        policy == RevocationPolicy::Online ? WTD_REVOKE_WHOLECHAIN : WTD_REVOKE_NONE;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &file;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = policy == RevocationPolicy::Offline ? WTD_CACHE_ONLY_URL_RETRIEVAL
                                                           : WTD_REVOCATION_CHECK_CHAIN;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const LONG status = WinVerifyTrust(nullptr, &action, &data);
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &action, &data);
    SignatureStatus result = SignatureStatus::Invalid;
    if (status == ERROR_SUCCESS)
        result = SignatureStatus::Valid;
    else if (status == TRUST_E_NOSIGNATURE || status == TRUST_E_SUBJECT_FORM_UNKNOWN ||
             status == TRUST_E_PROVIDER_UNKNOWN)
        result = SignatureStatus::Unsigned;
    else if (status == CERT_E_UNTRUSTEDROOT || status == TRUST_E_EXPLICIT_DISTRUST)
        result = SignatureStatus::Untrusted;
    else if (status == CERT_E_REVOCATION_FAILURE || status == CRYPT_E_REVOCATION_OFFLINE)
        result = SignatureStatus::RevocationUnavailable;
    return SignatureVerification{result, status};
}
Result<UpdateHandoffPlan> PrepareUpdateHandoff(const std::filesystem::path& candidate,
                                               const std::filesystem::path& target,
                                               const std::filesystem::path& backup,
                                               const std::filesystem::path& restart,
                                               std::vector<std::wstring> arguments,
                                               const UpdateVerificationPolicy& policy) {
    std::error_code filesystem_error;
    if (!std::filesystem::is_regular_file(candidate, filesystem_error) || filesystem_error ||
        !std::filesystem::is_regular_file(target, filesystem_error) || filesystem_error ||
        target.empty() || backup.empty() || restart.empty() || target == backup ||
        candidate == target)
        return NativeError::FromWin32(ERROR_INVALID_PARAMETER);
    auto target_volume = Volume(target), backup_volume = Volume(backup);
    if (!target_volume) return target_volume.Error();
    if (!backup_volume ||
        _wcsicmp(target_volume.Value().c_str(), backup_volume.Value().c_str()) != 0)
        return NativeError::FromWin32(ERROR_NOT_SAME_DEVICE);
    if (policy.require_valid_signature) {
        auto signature = VerifyAuthenticode(candidate, policy.revocation);
        if (!signature) return signature.Error();
        if (signature.Value().status != SignatureStatus::Valid)
            return NativeError::FromHResult(TRUST_E_SUBJECT_NOT_TRUSTED)
                .WithOperation(L"Update candidate signature");
    }
    if (MinimumSpecified(policy.minimum_version)) {
        auto version = QueryFileVersion(candidate);
        if (!version) return version.Error();
        if (version.Value() < policy.minimum_version)
            return NativeError::FromWin32(ERROR_PRODUCT_VERSION);
    }
    UpdateHandoffPlan plan;
    plan.candidate_ = std::filesystem::absolute(candidate);
    plan.target_ = std::filesystem::absolute(target);
    plan.backup_ = std::filesystem::absolute(backup);
    plan.restart_executable_ = std::filesystem::absolute(restart);
    plan.restart_arguments_ = std::move(arguments);
    plan.policy_ = policy;
    return plan;
}
Result<void> ApplyUpdateHandoff(const UpdateHandoffPlan& plan, DWORD process_id,
                                std::chrono::milliseconds timeout, std::stop_token stop) {
    if (process_id) {
        KernelHandle process(OpenProcess(SYNCHRONIZE, FALSE, process_id));
        if (!process) return NativeError::LastWin32();
        auto waited = WaitForHandle(process.Get(), timeout, stop);
        if (!waited) return waited.Error();
        if (waited.Value().status == WaitStatus::Timeout)
            return NativeError::FromWin32(WAIT_TIMEOUT);
        if (waited.Value().status == WaitStatus::Cancelled)
            return NativeError::FromWin32(ERROR_CANCELLED);
    }
    // A plan is immutable, but its candidate file is not. Re-validate at execution time and
    // again after staging so a post-plan replacement cannot bypass the selected policy.
    if (plan.Policy().require_valid_signature) {
        auto signature = VerifyAuthenticode(plan.Candidate(), plan.Policy().revocation);
        if (!signature) return signature.Error();
        if (signature.Value().status != SignatureStatus::Valid)
            return NativeError::FromHResult(TRUST_E_SUBJECT_NOT_TRUSTED)
                .WithOperation(L"Update candidate signature changed");
    }
    if (MinimumSpecified(plan.Policy().minimum_version)) {
        auto version = QueryFileVersion(plan.Candidate());
        if (!version) return version.Error();
        if (version.Value() < plan.Policy().minimum_version)
            return NativeError::FromWin32(ERROR_PRODUCT_VERSION);
    }
    auto staging = plan.Target();
    staging += L".mwfl-stage";
    if (!CopyFileW(plan.Candidate().c_str(), staging.c_str(), TRUE))
        return NativeError::LastWin32().WithOperation(L"Stage update");
    if (plan.Policy().require_valid_signature) {
        auto staged_signature = VerifyAuthenticode(staging, plan.Policy().revocation);
        if (!staged_signature || staged_signature.Value().status != SignatureStatus::Valid) {
            DeleteFileW(staging.c_str());
            return staged_signature
                       ? Result<void>{NativeError::FromHResult(TRUST_E_SUBJECT_NOT_TRUSTED)
                                          .WithOperation(L"Staged update signature")}
                       : Result<void>{staged_signature.Error()};
        }
    }
    DeleteFileW(plan.Backup().c_str());
    if (std::filesystem::exists(plan.Target()) &&
        !MoveFileExW(plan.Target().c_str(), plan.Backup().c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(staging.c_str());
        return NativeError::LastWin32().WithOperation(L"Backup update target");
    }
    if (!MoveFileExW(staging.c_str(), plan.Target().c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const auto error = NativeError::LastWin32().WithOperation(L"Commit update");
        MoveFileExW(plan.Backup().c_str(), plan.Target().c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        DeleteFileW(staging.c_str());
        return error;
    }
    std::wstring command = Quote(plan.RestartExecutable().wstring());
    for (const auto& arg : plan.RestartArguments()) command += L" " + Quote(arg);
    std::vector<wchar_t> mutable_command(command.begin(), command.end());
    mutable_command.push_back(L'\0');
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(plan.RestartExecutable().c_str(), mutable_command.data(), nullptr, nullptr,
                        FALSE, 0, nullptr, nullptr, &startup, &info)) {
        const auto error = NativeError::LastWin32().WithOperation(L"Restart updated application");
        MoveFileExW(plan.Target().c_str(), staging.c_str(), MOVEFILE_REPLACE_EXISTING);
        MoveFileExW(plan.Backup().c_str(), plan.Target().c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        DeleteFileW(staging.c_str());
        return error;
    }
    CloseHandle(info.hThread);
    CloseHandle(info.hProcess);
    return {};
}
}  // namespace mwfl
