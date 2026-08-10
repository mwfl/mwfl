#include <mwfl/ribbon.h>

int main() {
    mwfl::RibbonCommandModel model;
    return model.Add({{1}, {1}}) ? 0 : 1;
}
