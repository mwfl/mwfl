#include <mwfl/message_pump.h>

#include <type_traits>

// WaitAwareMessagePump aliases its own handle storage through a span; copying
// or moving it must stay deleted so the alias can never dangle.
static_assert(!std::is_copy_constructible_v<mwfl::WaitAwareMessagePump>);
static_assert(!std::is_copy_assignable_v<mwfl::WaitAwareMessagePump>);
static_assert(!std::is_move_constructible_v<mwfl::WaitAwareMessagePump>);
static_assert(!std::is_move_assignable_v<mwfl::WaitAwareMessagePump>);

// MessageLoop owns the thread-current registration and the filter chain;
// copying or moving it would duplicate or orphan that registration.
static_assert(!std::is_copy_constructible_v<mwfl::MessageLoop>);
static_assert(!std::is_copy_assignable_v<mwfl::MessageLoop>);
static_assert(!std::is_move_constructible_v<mwfl::MessageLoop>);
static_assert(!std::is_move_assignable_v<mwfl::MessageLoop>);
