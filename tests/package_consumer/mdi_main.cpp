#include <mwfl/mdi.h>

int main() {
    mwfl::MdiWorkspaceModel model;
    return model.Add({{1}, L"Document"}) ? 0 : 1;
}
