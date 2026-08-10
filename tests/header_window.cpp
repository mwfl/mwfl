#include <mwfl/window.h>

class HeaderWindow final : public mwfl::Window<HeaderWindow> {
public:
    void BuildUI() {}
};

class HeaderSimpleWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {}
};
