# Native diagnostics

Demonstrates pre-sink redaction, TraceLogging ETW, bounded file rotation, and
explicit privacy-conscious `WithThreadInfo` minidump policy without installing
a crash handler. Thread metadata is retained without collecting full process
memory that may contain application or user secrets.
