#include <mwtl/ribbon.h>

int main() {
    mwtl::RibbonCommandModel model;
    return model.Add({{1}, {1}}) ? 0 : 1;
}
