# Change the public API

1. Confirm the capability belongs in `docs/scope-map.md`.
2. Update the smallest header under `include/mwfl/`; never expose an internal type.
3. Document ownership, thread affinity, units, failure, and event propagation.
4. Add or update the independent-header probe and an observable test.
5. Update `docs/api-index.json` and a compiled example when the usage pattern changes.
6. Run the change-selected checks and then the full Debug/Release suite.

Compatibility and promotion rules are defined in `docs/stability.md`.
