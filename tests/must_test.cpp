#include <mwfl/must.h>

#include <cstdlib>
#include <system_error>

int main() {
    ::SetLastError(ERROR_ACCESS_DENIED);
    try {
        mwfl::Must(false, "plain failure");
        return 1;
    } catch (const std::system_error& error) {
        if (error.code().value() != ERROR_GEN_FAILURE) return 2;
    }

    try {
        mwfl::MustInvoke([] {
            ::SetLastError(ERROR_FILE_NOT_FOUND);
            return false;
        }, "captured failure");
        return 3;
    } catch (const std::system_error& error) {
        if (error.code().value() != ERROR_FILE_NOT_FOUND) return 4;
    }

    if (!mwfl::MustInvoke([] { return true; }, "success")) return 5;

    try {
        mwfl::Must(false, ERROR_INVALID_DATA, "explicit failure");
        return 6;
    } catch (const mwfl::Error& error) {
        if (error.code().value() != ERROR_INVALID_DATA ||
            error.Operation() != "explicit failure" ||
            error.Location().line() == 0) return 7;
    }
    return EXIT_SUCCESS;
}
