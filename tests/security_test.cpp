#include <mwfl/security.h>
#include <cassert>
int main() {
    auto empty = mwfl::UnprotectForCurrentUser({}); assert(!empty);
    std::vector<std::byte> bytes{std::byte{1}, std::byte{2}};
    mwfl::SecureBytes secure(std::move(bytes)); assert(secure.Size() == 2); secure.Clear(); assert(secure.Size() == 0);
}
