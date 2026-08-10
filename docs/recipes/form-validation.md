# Build a validated form

Use `ValueBinding<Control, Value>` when the model must remain explicit. A
binding's `Pull()` reads and validates a candidate before updating the model;
`Push()` writes the model to the control.

The complete canonical implementation is `examples/form_binding/main.cpp`; a
smaller starting point is `templates/form-app/main.cpp`.

Keep bindings as window members because they refer to both controls and model
values. Check `Pull().accepted` before updating dependent UI. On rejection,
show the returned message and focus the invalid control. Do not assume mwfl has
an implicit global data context.

