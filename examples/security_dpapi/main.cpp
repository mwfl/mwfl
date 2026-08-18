#include <mwfl/security.h>
#include <cstring>
#include <iostream>
#include <string_view>
int wmain(int argc, wchar_t** argv) {
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") { std::wcout << L"Usage: mwfl_security_dpapi --self-test\n"; return 0; }
    constexpr char secret[] = "local-example-secret";
    auto bytes = std::span(reinterpret_cast<const std::byte*>(secret), sizeof(secret) - 1);
    auto protected_data = mwfl::ProtectForCurrentUser(bytes, L"mwfl.example"); if (!protected_data) return 1;
    auto restored = mwfl::UnprotectForCurrentUser(protected_data.Value(), L"mwfl.example"); if (!restored) return 2;
    const bool matches = restored.Value().WithView([&](auto view) {
        return view.size() == bytes.size() &&
               std::memcmp(view.data(), bytes.data(), bytes.size()) == 0;
    });
    if (!matches) return 3;
    restored.Value().Clear();
    std::wcout << L"current-user DPAPI protection and secure clearing passed\n"; return 0;
}
