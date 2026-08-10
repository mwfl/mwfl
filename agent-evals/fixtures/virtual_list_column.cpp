#include <mwfl/mwfl.h>

#include <memory>

namespace {

class FileModel final : public mwfl::VirtualListModel {
public:
    std::size_t GetRowCount() const noexcept override { return 2; }
    mwfl::ListItemId GetRowId(std::size_t row) const noexcept override {
        constexpr mwfl::ListItemId ids[]{{101}, {102}};
        return row < 2 ? ids[row] : mwfl::ListItemId{};
    }
    std::wstring GetCellText(std::size_t row, int column) const override {
        if (row >= 2) return {};
        if (column == 0) return row == 0 ? L"alpha.txt" : L"beta.txt";
        if (column == 1) return row == 0 ? L"12 KB" : L"48 KB";
        return {};
    }
};

class VirtualColumnEval final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this};
        ui.Add(list_, {200}, mwfl::RectDip{},
               mwfl::ListViewOptions{.virtual_data = true});
        mwfl::AddColumns(list_, {{L"Name", 240}, {L"Size", 120}});
        mwfl::Must(list_.SetVirtualModel(model_), "attach file model");
        SetLayout(mwfl::Column().Add(list_, mwfl::Stretch()));
    }

    mwfl::EventResult OnNotify(const mwfl::NotifyEvent& event) override {
        if (!event.IsFrom(list_)) return mwfl::EventResult::Propagate();
        LRESULT result = 0;
        if (!list_.HandleNotification(event.header, result))
            return mwfl::EventResult::Propagate();
        if (auto error = list_.TakeVirtualException()) std::rethrow_exception(error);
        return mwfl::EventResult::Handled(result);
    }

private:
    std::shared_ptr<FileModel> model_ = std::make_shared<FileModel>();
    mwfl::ListView list_;
};

}  // namespace
