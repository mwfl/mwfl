#include <mwfl/security.h>
#include <iostream>
int wmain(int argc, wchar_t** argv) {
    const std::wstring target = L"mwfl/example/" + std::to_wstring(GetCurrentProcessId());
    if (argc == 2 && std::wstring_view(argv[1]) == L"--integration") { (void)mwfl::CredentialManager::Remove(target); const char value[] = "temporary"; mwfl::GenericCredential credential{target, L"mwfl", mwfl::SecureBuffer(std::span(reinterpret_cast<const std::byte*>(value), sizeof(value) - 1))}; auto written = mwfl::CredentialManager::Write(std::move(credential)); if (!written) { (void)mwfl::CredentialManager::Remove(target); return 1; } auto read = mwfl::CredentialManager::Read(target); auto removed = mwfl::CredentialManager::Remove(target); auto audit = mwfl::CredentialManager::Read(target); return read && removed && removed.Value() && !audit ? 0 : 2; }
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") return 0;
    auto identity = mwfl::QueryCurrentProcessIdentity(); if (!identity || identity.Value().user_sid.empty()) return 1;
    mwfl::SecureString secret(L"ephemeral");
    if (!secret.WithView([](std::wstring_view value) { return value == L"ephemeral"; })) return 2;
    secret.Clear();
    std::wcout << L"secure secret and token identity passed\n"; return 0;
}
