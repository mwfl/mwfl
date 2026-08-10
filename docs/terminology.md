# Natural-language and framework terminology

Use this map before guessing an API name.

| User or framework term | mwfl term |
|---|---|
| app/window entry point | `wWinMain` + `RunApplication` |
| widget/control | native control wrapper |
| text box/input | `TextBox` |
| static text/caption | `Label` |
| select/dropdown | `ComboBox` |
| checkbox/toggle | `CheckBox` |
| action | `Command` |
| action collection | `CommandSet` |
| vertical stack/flex column | `Column` |
| horizontal stack/flex row | `Row` |
| layers/z-stack | `Overlay` |
| fill/weighted size | `Stretch` |
| intrinsic/content size | `Auto` |
| density-independent pixel | `Dip`, `_dip` |
| signal/slot callback | typed `WindowBase` event override |
| dispatcher/invoke on UI | `WindowWakeup` + `OnWakeup` |
| data binding | explicit `ValueBinding` push/pull |
| native handle | `GetHwnd()` |
| default event processing | `EventResult::Propagate()` |
| consume event | `EventResult::Handled()` |

mwfl deliberately does not provide a virtual DOM, reflection-based property
system, implicit global data context, or general-purpose task scheduler.

