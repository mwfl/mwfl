#include <mwfl/deployment.h>
#include <cassert>
#include <string>
int main() {
    auto identity = mwfl::QueryCurrentPackageIdentity(); assert(identity);
    std::wstring too_long(RESTART_MAX_CMD_LINE, L'x'); auto invalid = mwfl::RegisterApplicationRestart(too_long); assert(!invalid);
}
