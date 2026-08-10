# mwtl 0.1.0：用现代 C++20，重新组织原生 Windows UI 开发

Windows 桌面开发从来不缺能力。Win32 API 稳定、成熟、覆盖广泛，真实的 HWND
可以与系统控件、Shell、COM、Direct2D、Direct3D、WebView2 以及大量既有组件直接协作。
真正让人犹豫的，往往是使用这些能力时必须反复处理的样板代码：窗口类注册、消息分发、
句柄生命周期、DPI 换算、控件布局、失败检查，以及后台线程与 UI 线程之间的交接。

这正是 **mwtl（Modern Windows Thin Layer）** 想解决的问题。

mwtl 不是新的渲染引擎，也不试图把 Windows 藏在一个封闭框架之后。它是一层面向
Windows 10 及以上系统、使用 C++20 和 Microsoft Visual C++ 的轻量原生 UI 库：保留
真实窗口、真实消息和真实返回值，同时把重复而容易出错的基础设施组织成更短、更明确、
更适合现代 C++ 与 Coding Agent 使用的 API。

今天，我们发布 **mwtl 0.1.0**，这是项目的第一个公开试用版本。

## 为什么还要做一个原生 Windows UI 库

很多 Windows UI 方案要求开发者在两种极端之间选择：完全直接使用 Win32，获得最大的
控制力但承担大量机械工作；或者采用更完整的应用框架，获得更高层的模型，同时接受它的
对象体系、生命周期、渲染方式和项目结构。

mwtl 选择中间的位置。它只抽象那些能够明显改善安全性和开发体验的部分：

- 用 RAII 表达窗口、控件、菜单、计时器和系统资源的所有权；
- 用类型化事件替代消息映射宏和重复的 `wParam`、`lParam` 解码；
- 用 DIP 与 Per-Monitor V2 布局处理现代高 DPI 窗口；
- 用明确的结果、错误和取消状态替代模糊的控制流；
- 用安全的唤醒对象处理后台线程到 UI 线程的交接；
- 在每一层保留 `GetHwnd()`、原生消息和 Windows SDK 互操作出口。

如果应用需要调用一个 mwtl 尚未封装的 Windows API，不需要等待框架支持，也不需要绕过
框架。拿到 HWND，继续使用原生 API 即可。这种“薄”不是功能少，而是不建立第二套世界。

## 一扇窗口可以有多简单

下面是一段完整的 mwtl 窗口 UI：

```cpp
#include <mwtl/mwtl.h>

using mwtl::operator""_dip;

class MainWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Hello, mwtl");

        mwtl::ControlHost ui{*this};
        ui.Add(name_, L"");
        ui.Add(greet_, L"Say hello");

        SetLayout(mwtl::Column()
            .Margin(24_dip)
            .Gap(12_dip)
            .Add(name_, mwtl::Fixed(32_dip))
            .Add(greet_, mwtl::Fixed(36_dip)));
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (event.IsClicked(greet_)) {
            SetTitle(L"Hello, " + name_.GetText());
            return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

private:
    mwtl::TextBox name_;
    mwtl::Button greet_;
};
```

这里没有消息映射宏、生成的应用对象或虚拟控件树。`TextBox` 和 `Button` 拥有真实 HWND；
布局使用 DIP；命令事件可以直接匹配控件；任何时候仍然可以通过 `GetHwnd()` 使用原生接口。

## 0.1.0 已经包含什么

首个版本不是只有一个 Hello World。仓库包含 **41 个有文档的能力切片、43 个可编译示例、
34 个面向 Coding Agent 的任务评测**，以及从小型示例到完整参考应用的连续学习路径。

基础能力包括应用生命周期、窗口、类型化键盘/鼠标/命令事件、计时器、工作线程唤醒、
Per-Monitor DPI、响应式 Row/Column/Overlay 布局，以及常用 Windows 控件。进一步的桌面能力
覆盖菜单、工具栏、状态栏、TreeView、ListView、属性页、任务对话框、托盘图标、剪贴板、
拖放、打印、图像解码、Shell 集成、单实例应用和设置持久化。

仓库还提供 Notepad、Explorer 风格界面、多文档工作区、Ribbon、MDI、IDE 风格 Docking、
绘图、图片浏览、打印、OLE 拖放等参考应用。WebView2 和 Scintilla 作为可选组件独立存在，
不会让核心 `mwtl::mwtl` 目标承担不需要的依赖。

## 为 Coding Agent 设计，而不只是“能被搜索到”

现代库的使用者不仅是人，也包括 Codex、Claude Code 等 Coding Agent。仅有一份长 API
手册并不足以让 Agent 稳定地生成正确代码；它还需要清楚知道某个任务对应哪些头文件、
符号、约束、示例和测试。

因此 mwtl 把 Agent 可用性作为仓库结构的一部分：

- `AGENTS.md` 定义开发约束、仓库地图和验证规则；
- `docs/api-index.json` 把用户任务映射到公开符号、示例、测试与注意事项；
- `docs/capabilities.json` 和 `docs/examples.json` 提供机器可读的能力目录；
- `docs/llms.txt` 与 `docs/llms-full.txt` 提供不同上下文预算的入口；
- 34 个 Agent eval fixture 确保任务能够只依赖公开 API 完成；
- 网站 Components Catalog 可以按任务、符号或头文件检索全部能力。

这些元数据不是旁路文档。CI 会检查每个公开能力是否能追溯到真实源码、CMake 目标、示例、
教程和测试，从而减少文档与实现逐渐分离的问题。

## 我们如何验证第一个公开版本

0.1.0 的主要开发与发布门禁是 Visual Studio 2026、Microsoft Visual C++、C++20 和 x64。
核心 Debug 与 Release 各通过 162 项测试；启用 WebView2 与 Scintilla 后，Debug 与 Release
各通过 170 项测试。覆盖范围包括独立头文件编译、单元测试、原生窗口生命周期、GUI
self-test、资源预算、安装包消费、文档、元数据和完整示例构建。

正式发布资产由同一标签自动构建，当前提供经过测试的 x64 ZIP 与 SHA-256 校验文件。
项目源码仍保留 x64/ARM64 目标，但 0.1.0 的公开二进制门禁刻意聚焦 x64。

## 适合谁，也不适合谁

mwtl 适合希望构建轻量、原生、可长期维护的 Windows 桌面程序，并希望继续直接使用
Windows SDK 的 C++ 团队。它尤其适合工具软件、内部桌面应用、编辑器、系统托盘程序、
原生 Shell 集成应用，以及需要与既有 HWND/COM 组件组合的项目。

它不是跨平台 UI 框架，不提供自定义渲染的虚拟控件体系，也不追求兼容旧 Windows、x86、
历史消息映射语法或其他框架的类层级。当前要求 Windows 10 1809+、C++20、Visual Studio
2022/2026 和 Microsoft Visual C++。

## 五分钟开始试用

使用 CMake `FetchContent`：

```cmake
include(FetchContent)
FetchContent_Declare(
    mwtl
    GIT_REPOSITORY https://github.com/everettjf/mwtl.git
    GIT_TAG v0.1.0)
FetchContent_MakeAvailable(mwtl)

add_executable(my_app WIN32 main.cpp)
target_compile_features(my_app PRIVATE cxx_std_20)
target_link_libraries(my_app PRIVATE mwtl::mwtl)
```

然后使用 Visual Studio 2026 构建 x64 应用。第一次接触原生 Windows 开发，可以从网站的
逐步教程开始；希望直接了解能力边界，可以打开 Components Catalog；希望让 Coding Agent
生成代码，可以先把 `docs/agent-usage.md` 交给它。

## 这只是公开试用的开始

0.1.0 的目标不是宣布 API 已经终结，而是建立一个足够完整、可测试、可讨论的公开基线。
下一阶段会继续围绕现代 Windows UI API 展开，包括 RichEdit、可滚动内容、聚焦的 GDI
所有权封装和剩余常用对话框。每一项能力都必须同时交付 API、示例、文档、Agent 元数据和
自动化证据。

如果你仍然喜欢原生 Windows 的能力，却希望用更现代、更简洁的 C++20 方式组织它，欢迎
试用 mwtl 0.1.0。Star、Issue、真实项目反馈，以及“这段代码还能不能更简单”的讨论，都会
帮助我们决定下一步。

- 项目主页：https://github.com/everettjf/mwtl
- 在线文档：https://everettjf.github.io/mwtl/
- 0.1.0 Release：https://github.com/everettjf/mwtl/releases/tag/v0.1.0
- 完整组件目录：https://everettjf.github.io/mwtl/components/catalog.html
