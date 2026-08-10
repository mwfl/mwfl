#include <mwfl/error.h>

#include <type_traits>

void HeaderErrorCompiles() {
    static_assert(std::is_base_of_v<std::system_error, mwfl::Error>);
}
