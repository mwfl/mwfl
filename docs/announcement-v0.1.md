# mwtl v0.1 发布：用现代 C++20 写原生 Windows UI

mwtl（Modern Windows Thin Layer）发布首个公开版本 v0.1。它面向希望保留
原生 Windows 体验、又不想重复编写大量 Win32 样板代码的 C++ 开发者。

mwtl 使用真实 HWND 和系统控件，提供类型化事件、明确的资源所有权、DPI
感知布局、桌面集成和安全的后台线程唤醒，同时在每一层保留直接使用 Win32
的能力。项目支持 Visual Studio 2022/2026、C++20、x64 和 ARM64，并附带
27 个可运行示例、完整 CI，以及面向 Codex、Claude Code 等 Coding Agent
的 API 导航、开发配方和自动评测。

如果你正在开发轻量、原生、可长期维护的 Windows 桌面程序，欢迎试用并反馈：
https://github.com/everettjf/mwtl
