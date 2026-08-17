#include <mwfl/core.h>
#include <mwfl/deployment.h>
#include <mwfl/diagnostics.h>
#include <mwfl/ipc.h>
#include <mwfl/process.h>
#include <mwfl/security.h>
#include <mwfl/service.h>
int main() { return mwfl::Utf8ToWide("foundation") ? 0 : 1; }
