#include <mwfl/app_support/update_checker.h>

int main() {
    using mwfl::app_support::IsVersionNewer;
    if (!IsVersionNewer(L"v1.2.0", L"1.1.9")) return 1;
    if (!IsVersionNewer(L"2.0", L"1.99.99")) return 2;
    if (IsVersionNewer(L"v1.2.0", L"1.2.0")) return 3;
    if (IsVersionNewer(L"1.1.9", L"1.2.0")) return 4;
    if (IsVersionNewer(L"invalid", L"0.0.0")) return 5;
    return 0;
}
