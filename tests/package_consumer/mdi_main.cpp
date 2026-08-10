#include <mwtl/mdi.h>

int main() {
    mwtl::MdiWorkspaceModel model;
    return model.Add({{1}, L"Document"}) ? 0 : 1;
}
