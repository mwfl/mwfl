#include <mwfl/binding.h>

#include <cstdlib>

namespace {
struct Field { int value = 0; };
}

int main() {
    Field field{4};
    int model = 7;
    mwfl::ValueBinding<Field, int> binding(
        field, model,
        [](const Field& value) { return value.value; },
        [](Field& value, const int& input) { value.value = input; return true; },
        [](const int& input) {
            return input >= 0 ? mwfl::ValidationResult::Accept()
                              : mwfl::ValidationResult::Reject(L"must be positive");
        });

    if (!binding.Push() || field.value != 7) return EXIT_FAILURE;
    field.value = 9;
    if (!binding.Pull().accepted || model != 9) return EXIT_FAILURE;
    field.value = -1;
    const auto rejected = binding.Pull();
    if (rejected.accepted || rejected.message.empty() || model != 9) return EXIT_FAILURE;
    {
        auto outer = binding.Suppress();
        auto inner = binding.Suppress();
        if (!binding.IsSuppressed() || !binding.Pull().accepted || model != 9)
            return EXIT_FAILURE;
    }
    return binding.IsSuppressed() ? EXIT_FAILURE : EXIT_SUCCESS;
}
