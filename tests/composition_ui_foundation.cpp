#include <mwfl/mwfl.h>
#include <mwfl/deployment.h>
#include <mwfl/security.h>
#include <system_error>
#include <type_traits>

static_assert(std::is_move_constructible_v<mwfl::SystemError>);
static_assert(std::is_base_of_v<std::system_error, mwfl::Error>);
