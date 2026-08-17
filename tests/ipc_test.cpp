#include <mwfl/ipc.h>
#include <cassert>
int main() {
    auto invalid = mwfl::ConnectPipe({L"not-a-pipe", 32});
    assert(!invalid && invalid.Error().code == ERROR_INVALID_PARAMETER);
    auto zero = mwfl::ConnectPipe({L"\\\\.\\pipe\\mwfl-invalid", 0});
    assert(!zero && zero.Error().code == ERROR_INVALID_PARAMETER);
}
