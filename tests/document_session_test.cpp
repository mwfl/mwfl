#include <mwfl/document_session.h>

#include <filesystem>

int main() {
    using namespace mwfl;
    DocumentWorkspaceModel model{{7}, 3};
    WorkspaceDocument first{{11}, L"Alpha \"unicode\" 文档", L"C:\\safe path\\a.txt",
                            true, true, false, {2, 8, 1, 1.25}};
    if (!model.Add(first) || !model.Add({{12}, L"Beta"}) || !model.Close({12})) return 1;
    DocumentSession session;
    session.workspaces.push_back(CaptureWorkspaceSession(model));
    session.workspaces[0].documents[0].stamp = FileStamp{4, 5, 6, 7};
    const auto text = SerializeDocumentSession(session);
    const auto parsed = ParseDocumentSession(text);
    if (!parsed || parsed.session->workspaces.size() != 1) return 2;
    const auto& restored = parsed.session->workspaces[0];
    if (restored.workspace != WorkspaceId{7} || restored.active != DocumentId{11} ||
        restored.documents.size() != 1 || restored.recently_closed.size() != 1 ||
        restored.documents[0].metadata.title != first.title ||
        restored.documents[0].metadata.path != first.path ||
        restored.documents[0].stamp != FileStamp{4, 5, 6, 7}) return 3;
    if (ParseDocumentSession(L"MWFL_DOCUMENT_SESSION 2 0\n").status !=
        DocumentSessionStatus::unsupported_version) return 4;
    if (ParseDocumentSession(text.substr(0, text.size() - 2)).status !=
        DocumentSessionStatus::malformed) return 5;
    DocumentSessionLimits tiny;
    tiny.maximum_bytes = 8;
    if (ParseDocumentSession(text, tiny).status != DocumentSessionStatus::oversized)
        return 6;
    auto duplicate = text;
    const auto marker = duplicate.find(L"R 12 ");
    if (marker == std::wstring::npos) return 7;
    duplicate.replace(marker + 2, 2, L"11");
    if (ParseDocumentSession(duplicate).status !=
        DocumentSessionStatus::duplicate_identity) return 8;
    if (!ParseDocumentSession(text + L"X optional future-field\n")) return 9;
    auto migrated_text = text;
    const auto version_marker = migrated_text.find(L" 1 ");
    if (version_marker == std::wstring::npos) return 15;
    migrated_text[version_marker + 1] = L'0';
    const auto migrated = ParseDocumentSession(migrated_text);
    if (!migrated || migrated.session->schema_version !=
                         document_session_schema_version) return 16;
    auto extended_text = text;
    extended_text.insert(extended_text.find(L"D "), L"X future workspace option\n");
    if (!ParseDocumentSession(extended_text)) return 17;

    const auto root = std::filesystem::temp_directory_path() /
        (L"mwfl-session-" + std::to_wstring(::GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    const auto path = root / L"session.mwfl";
    if (SaveDocumentSessionAtomic(path, session) != DocumentSessionStatus::success)
        return 10;
    const auto interrupted = path.wstring() + L".mwfl-tmp-interrupted";
    if (!WriteTextFileAtomic(interrupted, L"truncated session").Succeeded()) return 20;
    const auto loaded = LoadDocumentSession(path);
    if (!loaded || loaded.session->workspaces[0].documents[0].metadata.id != DocumentId{11})
        return 11;
    if (!WriteTextFileAtomic(path, L"MWFL_DOCUMENT_SESSION 1 1\nW 7").Succeeded() ||
        LoadDocumentSession(path).status != DocumentSessionStatus::malformed) return 21;
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    DocumentWorkspaceModel target{{7}, 3};
    const auto restored_result = RestoreWorkspaceSession(
        target, restored,
        [](const SessionDocument& item) {
            return item.metadata.id == DocumentId{12}
                ? SessionDocumentDisposition::missing
                : SessionDocumentDisposition::restore;
        },
        [](const SessionDocument&) { return true; });
    if (!restored_result || restored_result.status != SessionRestoreStatus::partial_success ||
        restored_result.restored_documents != 1 ||
        restored_result.restored_recently_closed != 0 ||
        restored_result.issues.size() != 1 || target.GetCount() != 1 ||
        !target.GetRecentlyClosed().empty()) return 12;
    DocumentWorkspaceModel throwing_target{{7}};
    const auto throwing = RestoreWorkspaceSession(
        throwing_target, restored,
        [](const SessionDocument&) -> SessionDocumentDisposition { throw 1; },
        [](const SessionDocument&) { return true; });
    if (!throwing || throwing.status != SessionRestoreStatus::partial_success ||
        throwing.issues.size() != 2 || !throwing.error || throwing_target.GetCount() != 0)
        return 13;
    DocumentWorkspaceModel occupied{{7}};
    occupied.Add({{99}, L"Existing"});
    if (RestoreWorkspaceSession(occupied, restored,
            [](const SessionDocument&) { return SessionDocumentDisposition::restore; },
            [](const SessionDocument&) { return true; }).status !=
        SessionRestoreStatus::target_not_empty) return 14;
    DocumentWorkspaceModel reentrant_target{{7}};
    const auto reentrant = RestoreWorkspaceSession(
        reentrant_target, restored,
        [](const SessionDocument&) { return SessionDocumentDisposition::restore; },
        [&](const SessionDocument&) {
            reentrant_target.Add({{99}, L"Reentrant"});
            return true;
        });
    if (!reentrant || reentrant.status != SessionRestoreStatus::partial_success ||
        reentrant.restored_documents != 0 || reentrant.issues.empty() ||
        !reentrant_target.Find({99})) return 18;

    auto invalid_restore = restored;
    invalid_restore.recently_closed[0].metadata.path = first.path;
    DocumentWorkspaceModel invalid_target{{7}};
    bool callback_called = false;
    const auto invalid_result = RestoreWorkspaceSession(
        invalid_target, invalid_restore,
        [&](const SessionDocument&) {
            callback_called = true;
            return SessionDocumentDisposition::restore;
        }, [](const SessionDocument&) { return true; });
    if (invalid_result.status != SessionRestoreStatus::invalid_argument || callback_called ||
        invalid_target.GetCount() != 0) return 19;
    return 0;
}
