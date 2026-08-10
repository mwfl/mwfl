#include <mwfl/ole_drag_drop.h>

Microsoft::WRL::ComPtr<IDropTarget> BuildFixtureTarget() {
    return mwfl::CreateOleDropTarget({
        .enter = [](IDataObject&, DWORD keys, POINTL, DWORD allowed) {
            return mwfl::NegotiateDropEffect(allowed, keys, DROPEFFECT_COPY);
        },
        .over = [](DWORD keys, POINTL, DWORD allowed) {
            return mwfl::NegotiateDropEffect(allowed, keys, DROPEFFECT_COPY);
        },
        .leave = [] {},
        .drop = [](IDataObject&, DWORD keys, POINTL, DWORD allowed) {
            return mwfl::NegotiateDropEffect(allowed, keys, DROPEFFECT_COPY);
        }});
}
