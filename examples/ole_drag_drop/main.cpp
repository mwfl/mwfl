#include <mwtl/mwtl.h>
#include <mwtl/ole_drag_drop.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using mwtl::operator""_dip;

namespace {
constexpr mwtl::ControlId kSource{861};
constexpr mwtl::ControlId kDestination{862};
constexpr mwtl::ControlId kCopy{863};
constexpr mwtl::ControlId kMove{864};
constexpr mwtl::ControlId kClear{865};
constexpr UINT kRunSelfTest = WM_APP + 0x1C0;
bool g_self_test = false;

class DragDropWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"mwtl OLE Drag and Drop");
        ::SetWindowPos(GetHwnd(), nullptr, 0, 0, 980, 720,
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        custom_format_ = mwtl::RegisterOleFormat(L"mwtl.example.item.v1");
        if (!custom_format_) throw std::runtime_error("register custom OLE format failed");

        mwtl::ControlHost ui{*this};
        ui.Add(title_, L"OLE drag/drop — Unicode, files, and an application-defined format");
        ui.Add(source_label_, L"Source items");
        ui.Add(source_, kSource, {0.0_dip, 0.0_dip, 300.0_dip, 300.0_dip});
        ui.Add(destination_label_, L"Dropped items (also accepts Explorer and other apps)");
        ui.Add(destination_, kDestination, {0.0_dip, 0.0_dip, 300.0_dip, 300.0_dip});
        ui.Add(copy_, kCopy, L"Copy selected (Ctrl+Enter)",
               {0.0_dip, 0.0_dip, 180.0_dip, 32.0_dip});
        ui.Add(move_, kMove, L"Move selected (Shift+Enter)",
               {0.0_dip, 0.0_dip, 180.0_dip, 32.0_dip});
        ui.Add(clear_, kClear, L"Clear destination", {0.0_dip, 0.0_dip, 140.0_dip, 32.0_dip});
        ui.Add(status_, L"Select an item, then drag from the grip below or use the buttons.");
        for (std::wstring_view item : {L"Alpha — 中文", L"Beta — café", L"Gamma — Ελληνικά"})
            source_.AddItem(item);
        source_.SetSelection(0);
        SetLayout(mwtl::Column()
                      .Margin(18.0_dip)
                      .Gap(10.0_dip)
                      .Add(title_, mwtl::Fixed(34.0_dip))
                      .Add(mwtl::Row().Gap(18.0_dip)
                               .Add(mwtl::Column().Gap(6.0_dip)
                                        .Add(source_label_, mwtl::Fixed(26.0_dip))
                                        .Add(source_, mwtl::Fixed(330.0_dip)),
                                    mwtl::Stretch())
                               .Add(mwtl::Column().Gap(6.0_dip)
                                        .Add(destination_label_, mwtl::Fixed(26.0_dip))
                                        .Add(destination_, mwtl::Fixed(330.0_dip)),
                                    mwtl::Stretch()),
                           mwtl::Fixed(362.0_dip))
                      .Add(mwtl::Row().Gap(8.0_dip)
                               .Add(copy_, mwtl::Auto())
                               .Add(move_, mwtl::Auto())
                               .Add(clear_, mwtl::Auto()),
                           mwtl::Fixed(38.0_dip))
                      .Add(status_, mwtl::Fixed(32.0_dip)));
        mwtl::Must(mwtl::SetAccessibleName(source_.GetHwnd(), L"Drag source items"),
                   "name OLE source list");
        mwtl::Must(mwtl::SetAccessibleName(destination_.GetHwnd(), L"Drop destination items"),
                   "name OLE destination list");
        mwtl::ApplyWindowAppearance(GetHwnd());

        target_ = mwtl::CreateOleDropTarget({
            .enter = [this](IDataObject& data, DWORD, POINTL, DWORD allowed) {
                status_.SetText(L"Drag entered — release to drop; Esc cancels.");
                return Supports(data) ? PreferredEffect(allowed) : DROPEFFECT_NONE;
            },
            .over = [](DWORD, POINTL, DWORD allowed) {
                return PreferredEffect(allowed);
            },
            .leave = [this] { status_.SetText(L"Drag cancelled or left the window."); },
            .drop = [this](IDataObject& data, DWORD, POINTL, DWORD allowed) {
                return Accept(data) ? PreferredEffect(allowed) : DROPEFFECT_NONE;
            }});
        auto registered = mwtl::RegisterOleDropTarget(GetHwnd(), target_);
        if (!registered) throw std::runtime_error("register OLE drop target failed");
        registration_ = std::move(registered.registration);
        if (g_self_test && !::PostMessageW(GetHwnd(), kRunSelfTest, 0, 0))
            throw std::runtime_error("post OLE drag/drop self-test failed");
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (event.IsClicked(copy_)) { TransferSelected(false); return mwtl::EventResult::Handled(); }
        if (event.IsClicked(move_)) { TransferSelected(true); return mwtl::EventResult::Handled(); }
        if (event.IsClicked(clear_)) {
            destination_.ClearItems();
            status_.SetText(L"Destination cleared.");
            return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnKeyDown(const mwtl::KeyEvent& event) override {
        if (event.virtual_key == VK_RETURN && (::GetKeyState(VK_CONTROL) & 0x8000)) {
            TransferSelected(false); return mwtl::EventResult::Handled();
        }
        if (event.virtual_key == VK_RETURN && (::GetKeyState(VK_SHIFT) & 0x8000)) {
            TransferSelected(true); return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnLeftButtonDown(const mwtl::MouseEvent& event) override {
        const RECT grip = DragGrip();
        if (::PtInRect(&grip, event.position)) {
            drag_armed_ = true;
            drag_start_ = event.position;
            ::SetCapture(GetHwnd());
            return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnMouseMove(const mwtl::MouseEvent& event) override {
        if (drag_armed_ && (event.key_state & MK_LBUTTON)) {
            const int threshold_x = ::GetSystemMetrics(SM_CXDRAG);
            const int threshold_y = ::GetSystemMetrics(SM_CYDRAG);
            if (std::abs(event.position.x - drag_start_.x) >= threshold_x ||
                std::abs(event.position.y - drag_start_.y) >= threshold_y) {
                drag_armed_ = false;
                if (::GetCapture() == GetHwnd()) ::ReleaseCapture();
                StartMouseDrag();
            }
            return mwtl::EventResult::Handled();
        }
        if (drag_armed_ && !(event.key_state & MK_LBUTTON)) {
            drag_armed_ = false;
            if (::GetCapture() == GetHwnd()) ::ReleaseCapture();
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnPaint(mwtl::PaintEvent& event) override {
        RECT grip = DragGrip();
        HBRUSH brush = ::CreateSolidBrush(::GetSysColor(COLOR_HIGHLIGHT));
        if (brush) { ::FillRect(event.GetDC(), &grip, brush); ::DeleteObject(brush); }
        ::SetBkMode(event.GetDC(), TRANSPARENT);
        ::SetTextColor(event.GetDC(), ::GetSysColor(COLOR_HIGHLIGHTTEXT));
        ::DrawTextW(event.GetDC(), L"Drag selected item from here", -1, &grip,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        return mwtl::EventResult::Handled();
    }

    mwtl::EventResult OnMessage(const mwtl::WindowMessage& event) override {
        if (event.id == kRunSelfTest) { RunSelfTest(); return mwtl::EventResult::Handled(); }
        if (event.id == WM_THEMECHANGED || event.id == WM_SYSCOLORCHANGE ||
            event.id == WM_SETTINGCHANGE) {
            mwtl::ApplyWindowAppearance(GetHwnd());
            ::InvalidateRect(GetHwnd(), nullptr, TRUE);
            return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnClose() override {
        if (registration_.IsRegistered()) registration_.Revoke();
        return mwtl::EventResult::Propagate();
    }

private:
    static DWORD PreferredEffect(DWORD allowed) {
        return (allowed & DROPEFFECT_COPY) ? DROPEFFECT_COPY :
               ((allowed & DROPEFFECT_MOVE) ? DROPEFFECT_MOVE : DROPEFFECT_NONE);
    }

    RECT DragGrip() const {
        RECT client{};
        ::GetClientRect(GetHwnd(), &client);
        const int margin = 18;
        return {client.left + margin, client.bottom - 70, client.right - margin,
                client.bottom - 24};
    }

    bool Supports(IDataObject& data) const {
        for (CLIPFORMAT format : {custom_format_, static_cast<CLIPFORMAT>(CF_UNICODETEXT),
                                  static_cast<CLIPFORMAT>(CF_HDROP)}) {
            FORMATETC descriptor{format, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
            if (SUCCEEDED(data.QueryGetData(&descriptor))) return true;
        }
        return false;
    }

    Microsoft::WRL::ComPtr<IDataObject> BuildSelectedData() const {
        const auto selected = source_.GetSelectedIndex();
        if (!selected) return {};
        const auto item = source_.GetItemText(*selected);
        if (!item) return {};
        mwtl::OleDataObjectBuilder builder;
        builder.AddUnicodeText(*item);
        std::vector<std::byte> custom((item->size() + 1) * sizeof(wchar_t));
        std::memcpy(custom.data(), item->c_str(), custom.size());
        builder.AddBytes(custom_format_, custom);
        const std::array<std::filesystem::path, 1> files{
            std::filesystem::path(L"C:\\mwtl-demo\\") / (*item + L".txt")};
        builder.AddFiles(files);
        return builder.Build();
    }

    bool Accept(IDataObject& data) {
        const auto custom = mwtl::ReadOleBytes(data, custom_format_);
        if (custom && custom.bytes.size() >= sizeof(wchar_t) &&
            custom.bytes.size() % sizeof(wchar_t) == 0) {
            const auto* text = reinterpret_cast<const wchar_t*>(custom.bytes.data());
            const std::size_t count = custom.bytes.size() / sizeof(wchar_t);
            const auto* end = std::find(text, text + count, L'\0');
            if (end != text + count) {
                destination_.AddItem(L"Custom: " + std::wstring(text, end));
                status_.SetText(L"Accepted application-defined format.");
                return true;
            }
        }
        const auto text = mwtl::ReadOleUnicodeText(data);
        if (text) {
            destination_.AddItem(L"Text: " + text.text);
            status_.SetText(L"Accepted Unicode text.");
            return true;
        }
        const auto files = mwtl::ReadOleFiles(data);
        if (files) {
            for (const auto& file : files.files) destination_.AddItem(L"File: " + file.native());
            status_.SetText(L"Accepted files from an OLE source.");
            return true;
        }
        status_.SetText(L"No supported format in this drop.");
        return false;
    }

    void TransferSelected(bool move) {
        const auto selected = source_.GetSelectedIndex();
        const auto item = selected ? source_.GetItemText(*selected) : std::nullopt;
        if (!selected || !item) { status_.SetText(L"Select a source item first."); return; }
        destination_.AddItem((move ? L"Keyboard move: " : L"Keyboard copy: ") + *item);
        if (move) {
            source_.RemoveItem(*selected);
            const int remaining = source_.GetItemCount();
            if (remaining > 0) source_.SetSelection((std::min)(*selected, remaining - 1));
        }
        status_.SetText(move ? L"Moved with keyboard-equivalent action."
                             : L"Copied with keyboard-equivalent action.");
    }

    void StartMouseDrag() {
        const auto selected = source_.GetSelectedIndex();
        auto data = BuildSelectedData();
        if (!selected || !data) { status_.SetText(L"Select a source item first."); return; }
        status_.SetText(L"Dragging — Ctrl copies, Shift moves, Esc cancels.");
        const auto result = mwtl::DoOleDragDrop(*data.Get(), DROPEFFECT_COPY | DROPEFFECT_MOVE);
        if (result.status == mwtl::OleDragStatus::success &&
            result.effect == DROPEFFECT_MOVE && source_.GetItemText(*selected)) {
            source_.RemoveItem(*selected);
            const int remaining = source_.GetItemCount();
            if (remaining > 0) source_.SetSelection((std::min)(*selected, remaining - 1));
        }
        status_.SetText(result.status == mwtl::OleDragStatus::cancelled
                            ? L"Drag cancelled safely."
                            : (result ? L"Drag completed." : L"Drag failed."));
    }

    void RunSelfTest() noexcept {
        int result = 0;
        try {
            auto data = BuildSelectedData();
            if (!data || !Supports(*data.Get())) result = 1;
            DWORD effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
            if (result == 0 && (FAILED(target_->DragEnter(data.Get(), 0, {10, 10}, &effect)) ||
                                effect != DROPEFFECT_COPY)) result = 2;
            effect = DROPEFFECT_COPY;
            if (result == 0 && (FAILED(target_->Drop(data.Get(), 0, {10, 10}, &effect)) ||
                                destination_.GetItemCount() != 1)) result = 3;
            mwtl::OleDataObjectBuilder external_text;
            external_text.AddUnicodeText(L"External Unicode 中文");
            auto text_data = external_text.Build();
            effect = DROPEFFECT_COPY;
            if (result == 0 && (FAILED(target_->DragEnter(text_data.Get(), 0, {}, &effect)) ||
                                FAILED(target_->Drop(text_data.Get(), 0, {}, &effect)) ||
                                destination_.GetItemCount() != 2)) result = 10;
            mwtl::OleDataObjectBuilder external_files;
            const std::array<std::filesystem::path, 2> paths{L"C:\\one.txt", L"D:\\two.txt"};
            external_files.AddFiles(paths);
            auto file_data = external_files.Build();
            effect = DROPEFFECT_COPY;
            if (result == 0 && (FAILED(target_->DragEnter(file_data.Get(), 0, {}, &effect)) ||
                                FAILED(target_->Drop(file_data.Get(), 0, {}, &effect)) ||
                                destination_.GetItemCount() != 4)) result = 11;
            ::SendMessageW(copy_.GetHwnd(), BM_CLICK, 0, 0);
            if (result == 0 && destination_.GetItemCount() != 5) result = 4;
            source_.SetSelection(1);
            ::SendMessageW(move_.GetHwnd(), BM_CLICK, 0, 0);
            if (result == 0 && (destination_.GetItemCount() != 6 ||
                                source_.GetItemCount() != 2)) result = 5;
            const auto before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
            for (int index = 0; result == 0 && index < 100; ++index) {
                auto repeated = BuildSelectedData();
                if (!repeated) { result = 6; break; }
            }
            const auto after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
            if (result == 0 && after > before + 1) result = 7;
            if (registration_.Revoke().status != mwtl::OleDragStatus::success ||
                registration_.Revoke().status != mwtl::OleDragStatus::not_registered)
                result = 8;
        } catch (...) {
            result = 9;
        }
        ::PostQuitMessage(result);
    }

    CLIPFORMAT custom_format_ = 0;
    bool drag_armed_ = false;
    POINT drag_start_{};
    Microsoft::WRL::ComPtr<IDropTarget> target_;
    mwtl::OleDropTargetRegistration registration_;
    mwtl::Label title_, source_label_, destination_label_, status_;
    mwtl::ListBox source_, destination_;
    mwtl::Button copy_, move_, clear_;
};
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    g_self_test = std::wstring_view{::GetCommandLineW()}.find(L"--self-test") !=
                  std::wstring_view::npos;
    return mwtl::RunApplication<DragDropWindow>(
        instance, show_command, {}, {.com_apartment = mwtl::ComApartment::ole_sta});
}
