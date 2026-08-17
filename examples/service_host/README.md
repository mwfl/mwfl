# Service host

Demonstrates the 0.1.2 Service lifecycle state machine and the console debug
host. The console host runs the same stop-token-aware callback used by a service
implementation without installing or mutating SCM state.

`mwfl_service_host --service` exercises the real SCM dispatcher when the
binary is explicitly installed as a test service. The example never installs
or removes itself.
