# Register a reversible file association

Use a dotted extension, stable ProgID, unique owner ID, executable, and verbs.
`RegisterPerUserFileAssociation` writes under HKCU and claims the extension
only after the ProgID is complete. It quotes `%1` automatically. Registration
is an explicit user action; removal must call `RemovePerUserFileAssociation`.
Removal refuses foreign ownership and preserves unrelated values. Tests use
`RegisterFileAssociation` with a unique isolated HKCU subkey and notification
disabled.
