# Save a typed versioned setting

Construct `VersionedSettingsStore` with a borrowed HKCU root, owned subkey, and
nonzero schema version. Save DWORD, QWORD, Unicode string, or binary values.
`SchemaVersion` is written last as the commit marker. Load with explicit names,
types, required flags, and byte limits; handle `not_found`, `version_mismatch`,
`malformed`, and `access_denied` separately. `RemoveOwned` deletes only listed
values plus the marker and preserves unrelated registry state.
