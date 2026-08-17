#include <mwfl/deployment.h>
#include <iostream>
#include <string_view>
int wmain(int argc, wchar_t** argv) {
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") { std::wcout << L"Usage: mwfl_deployment_restart --self-test\n"; return 0; }
    auto identity = mwfl::QueryCurrentPackageIdentity(); if (!identity) return 1;
    mwfl::RestartRegistration restart(L"--recover"); if (!restart.Active()) return 2;
    std::wcout << L"package identity and scoped restart registration passed (packaged=" << identity.Value().packaged << L")\n";
    return 0;
}
