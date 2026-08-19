#include <mwfl/app_support/update_checker.h>

int main() {
    return mwfl::app_support::IsVersionNewer(L"1.2.1", L"1.2.0") ? 0 : 1;
}
