# Share print-preview pagination

Store one vector of `PrintPage` values from `PaginateContent`. Use it for both
`PrintPreviewModel` selection and the native print callback. Preview zoom and
selected page are view state only; they never rewrite document content or page
identity. Render previews with screen/theme colors and print pages with printer
units from the supplied DC.

See `examples/printing/main.cpp` and run `mwtl.printing_gui`.
