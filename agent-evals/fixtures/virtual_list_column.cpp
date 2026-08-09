#include <mwtl/mwtl.h>

#include <memory>

namespace {

class FileModel final : public mwtl::VirtualListModel {
public:
    std::size_t GetRowCount() const noexcept override { return 2; }
    mwtl::ListItemId GetRowId(std::size_t row) const noexcept override {
        constexpr mwtl::ListItemId ids[]{{101}, {102}};
        return row < 2 ? ids[row] : mwtl::ListItemId{};
    }
    std::wstring GetCellText(std::size_t row, int column) const override {
        if (row >= 2) return {};
        if (column == 0) return row == 0 ? L"alpha.txt" : L"beta.txt";
        if (column == 1) return row == 0 ? L"12 KB" : L"48 KB";
        return {};
    }
};

class VirtualColumnEval final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        mwtl::ControlHost ui{*this};
        ui.Add(list_, {200}, mwtl::RectDip{},
               mwtl::ListViewOptions{.virtual_data = true});
        mwtl::AddColumns(list_, {{L"Name", 240}, {L"Size", 120}});
        mwtl::Must(list_.SetVirtualModel(model_), "attach file model");
        SetLayout(mwtl::Column().Add(list_, mwtl::Stretch()));
    }

    mwtl::EventResult OnNotify(const mwtl::NotifyEvent& event) override {
        if (!event.IsFrom(list_)) return mwtl::EventResult::Propagate();
        LRESULT result = 0;
        if (!list_.HandleNotification(event.header, result))
            return mwtl::EventResult::Propagate();
        if (auto error = list_.TakeVirtualException()) std::rethrow_exception(error);
        return mwtl::EventResult::Handled(result);
    }

private:
    std::shared_ptr<FileModel> model_ = std::make_shared<FileModel>();
    mwtl::ListView list_;
};

}  // namespace
