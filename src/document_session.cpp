#include <mwfl/document_session.h>

#include <iomanip>
#include <ranges>
#include <sstream>

namespace mwfl {
namespace {

constexpr std::wstring_view magic = L"MWFL_DOCUMENT_SESSION";

std::optional<std::size_t> Utf8Size(std::wstring_view text) {
    if (text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        return std::nullopt;
    const int size = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size == 0 && !text.empty()) return std::nullopt;
    return static_cast<std::size_t>(size);
}

bool ReadRecordLine(std::wistream& stream, std::wstring& line) {
    while (std::getline(stream, line)) {
        if (!line.starts_with(L"X ")) return true;
    }
    return false;
}

void WriteDocument(std::wostream& out, wchar_t tag, const SessionDocument& value) {
    const auto& d = value.metadata;
    out << tag << L' ' << d.id.value << L' ' << std::quoted(d.title) << L' '
        << std::quoted(d.path.native()) << L' ' << d.dirty << L' ' << d.can_undo
        << L' ' << d.can_redo << L' ' << d.view.selection_start << L' '
        << d.view.selection_end << L' ' << d.view.first_visible_unit << L' '
        << std::setprecision(17) << d.view.zoom << L' ' << value.stamp.has_value();
    if (value.stamp)
        out << L' ' << value.stamp->size << L' ' << value.stamp->last_write_time
            << L' ' << value.stamp->file_id << L' ' << value.stamp->volume_serial;
    out << L'\n';
}

bool ReadDocument(std::wstring_view line, SessionDocument& value,
                  const DocumentSessionLimits& limits) {
    std::wistringstream in{std::wstring{line}};
    bool has_stamp = false;
    std::wstring path;
    auto& d = value.metadata;
    if (!(in >> d.id.value >> std::quoted(d.title) >> std::quoted(path) >> d.dirty
          >> d.can_undo >> d.can_redo >> d.view.selection_start
          >> d.view.selection_end >> d.view.first_visible_unit >> d.view.zoom
          >> has_stamp)) return false;
    d.path = std::move(path);
    if (has_stamp) {
        FileStamp stamp;
        if (!(in >> stamp.size >> stamp.last_write_time >> stamp.file_id
              >> stamp.volume_serial)) return false;
        value.stamp = stamp;
    }
    in >> std::ws;
    return in.eof() && d.id && !d.title.empty() && d.view.IsValid() &&
           d.title.size() <= limits.maximum_string_units &&
           d.path.native().size() <= limits.maximum_string_units;
}

bool HasId(const WorkspaceSession& workspace, DocumentId id) {
    const auto contains = [=](const auto& list) {
        return std::ranges::any_of(list, [=](const auto& item) {
            return item.metadata.id == id;
        });
    };
    return contains(workspace.documents) || contains(workspace.recently_closed);
}

}  // namespace

WorkspaceSession CaptureWorkspaceSession(const DocumentWorkspaceModel& workspace) {
    WorkspaceSession result;
    result.workspace = workspace.GetId();
    result.active = workspace.GetActiveId();
    for (const auto& item : workspace.GetDocuments()) result.documents.push_back({item, {}});
    for (const auto& item : workspace.GetRecentlyClosed())
        result.recently_closed.push_back({item, {}});
    return result;
}

std::wstring SerializeDocumentSession(const DocumentSession& session) {
    std::wostringstream out;
    out << magic << L' ' << session.schema_version << L' '
        << session.workspaces.size() << L'\n';
    for (const auto& workspace : session.workspaces) {
        out << L"W " << workspace.workspace.value << L' '
            << workspace.active.value_or(DocumentId{}).value << L' '
            << workspace.documents.size() << L' '
            << workspace.recently_closed.size() << L'\n';
        for (const auto& item : workspace.documents) WriteDocument(out, L'D', item);
        for (const auto& item : workspace.recently_closed) WriteDocument(out, L'R', item);
        out << L"E\n";
    }
    return std::move(out).str();
}

DocumentSessionResult ParseDocumentSession(std::wstring_view text,
                                           const DocumentSessionLimits& limits) {
    const auto utf8_size = Utf8Size(text);
    if (!utf8_size) return {DocumentSessionStatus::malformed};
    if (*utf8_size > limits.maximum_bytes) return {DocumentSessionStatus::oversized};
    std::wistringstream stream{std::wstring{text}};
    std::wstring line;
    if (!std::getline(stream, line)) return {DocumentSessionStatus::malformed};
    std::wistringstream header{line};
    std::wstring parsed_magic;
    std::uint32_t version = 0;
    std::size_t workspace_count = 0;
    if (!(header >> parsed_magic >> version >> workspace_count) || parsed_magic != magic)
        return {DocumentSessionStatus::malformed};
    header >> std::ws;
    if (!header.eof()) return {DocumentSessionStatus::malformed};
    // Schema 0 used the same required records but had no defined extension
    // convention. Parsing it into the current in-memory schema is the only
    // migration currently required.
    if (version > document_session_schema_version)
        return {DocumentSessionStatus::unsupported_version};
    if (workspace_count > limits.maximum_workspaces)
        return {DocumentSessionStatus::oversized};
    DocumentSession result;
    for (std::size_t wi = 0; wi < workspace_count; ++wi) {
        if (!ReadRecordLine(stream, line)) return {DocumentSessionStatus::malformed};
        std::wistringstream workspace_line{line};
        wchar_t tag{};
        WorkspaceSession workspace;
        std::uint64_t active = 0;
        std::size_t document_count = 0, recent_count = 0;
        if (!(workspace_line >> tag >> workspace.workspace.value >> active
              >> document_count >> recent_count) || tag != L'W' || !workspace.workspace)
            return {DocumentSessionStatus::malformed};
        workspace_line >> std::ws;
        if (!workspace_line.eof()) return {DocumentSessionStatus::malformed};
        if (document_count > limits.maximum_documents_per_workspace ||
            recent_count > limits.maximum_documents_per_workspace ||
            document_count + recent_count > limits.maximum_documents_per_workspace)
            return {DocumentSessionStatus::oversized};
        if (active) workspace.active = DocumentId{active};
        for (std::size_t di = 0; di < document_count + recent_count; ++di) {
            if (!ReadRecordLine(stream, line) || line.size() < 2)
                return {DocumentSessionStatus::malformed};
            const auto expected = di < document_count ? L'D' : L'R';
            if (line[0] != expected || line[1] != L' ')
                return {DocumentSessionStatus::malformed};
            SessionDocument document;
            if (!ReadDocument(std::wstring_view{line}.substr(2), document, limits))
                return {DocumentSessionStatus::malformed};
            if (HasId(workspace, document.metadata.id))
                return {DocumentSessionStatus::duplicate_identity};
            (expected == L'D' ? workspace.documents : workspace.recently_closed)
                .push_back(std::move(document));
        }
        if (workspace.active && !std::ranges::any_of(workspace.documents,
                [&](const auto& item) { return item.metadata.id == workspace.active; }))
            return {DocumentSessionStatus::malformed};
        if (!ReadRecordLine(stream, line) || line != L"E")
            return {DocumentSessionStatus::malformed};
        if (std::ranges::any_of(result.workspaces, [&](const auto& item) {
                return item.workspace == workspace.workspace;
            })) return {DocumentSessionStatus::duplicate_identity};
        result.workspaces.push_back(std::move(workspace));
    }
    while (std::getline(stream, line)) {
        if (!line.empty() && !line.starts_with(L"X "))
            return {DocumentSessionStatus::malformed};
    }
    return {DocumentSessionStatus::success, std::move(result)};
}

DocumentSessionResult LoadDocumentSession(const std::filesystem::path& path,
                                           const DocumentSessionLimits& limits) {
    const auto read = ReadTextFile(path);
    if (!read.Succeeded())
        return {read.status == TextFileStatus::not_found ? DocumentSessionStatus::not_found
                                                        : DocumentSessionStatus::io_error,
                {}, read.native_error};
    if (read.value->stamp.size > limits.maximum_bytes)
        return {DocumentSessionStatus::oversized};
    return ParseDocumentSession(read.value->text, limits);
}

DocumentSessionStatus SaveDocumentSessionAtomic(const std::filesystem::path& path,
                                                const DocumentSession& session,
                                                const DocumentSessionLimits& limits) {
    const auto serialized = SerializeDocumentSession(session);
    const auto utf8_size = Utf8Size(serialized);
    if (!utf8_size) return DocumentSessionStatus::malformed;
    if (*utf8_size > limits.maximum_bytes) return DocumentSessionStatus::oversized;
    const auto checked = ParseDocumentSession(serialized, limits);
    if (!checked) return checked.status;
    return WriteTextFileAtomic(path, serialized).Succeeded()
        ? DocumentSessionStatus::success : DocumentSessionStatus::io_error;
}

SessionRestoreResult RestoreWorkspaceSession(
    DocumentWorkspaceModel& target, const WorkspaceSession& session,
    const SessionDocumentValidator& validate,
    const SessionDocumentRestoreHandler& restore) {
    SessionRestoreResult result;
    if (!session.workspace || session.workspace != target.GetId() || !validate || !restore) {
        result.status = SessionRestoreStatus::invalid_argument;
        return result;
    }
    if (target.GetCount() != 0 || !target.GetRecentlyClosed().empty()) {
        result.status = SessionRestoreStatus::target_not_empty;
        return result;
    }
    // Prove that every model mutation can succeed before application-owned
    // restoration callbacks acquire contents or native resources.
    DocumentWorkspaceModel probe{session.workspace, session.recently_closed.size()};
    for (const auto& item : session.documents) {
        if (!probe.Add(item.metadata)) {
            result.status = SessionRestoreStatus::invalid_argument;
            return result;
        }
    }
    std::vector<WorkspaceDocument> probe_recent;
    for (const auto& item : session.recently_closed)
        probe_recent.push_back(item.metadata);
    if (!probe.RestoreRecentlyClosed(probe_recent)) {
        result.status = SessionRestoreStatus::invalid_argument;
        return result;
    }
    std::vector<WorkspaceDocument> recent;
    const auto assess = [&](const SessionDocument& item, bool closed) {
        const auto revision = target.GetRevision();
        SessionDocumentDisposition disposition = SessionDocumentDisposition::rejected;
        try {
            disposition = validate(item);
            if (disposition == SessionDocumentDisposition::restore && !closed &&
                !restore(item)) disposition = SessionDocumentDisposition::rejected;
        } catch (...) {
            if (!result.error) result.error = std::current_exception();
            disposition = SessionDocumentDisposition::rejected;
        }
        if (target.GetRevision() != revision)
            disposition = SessionDocumentDisposition::rejected;
        if (disposition != SessionDocumentDisposition::restore) {
            result.issues.push_back({item.metadata.id, disposition, closed});
            return false;
        }
        return true;
    };
    for (const auto& item : session.documents) {
        if (!assess(item, false)) continue;
        const auto added = target.Add(item.metadata);
        if (!added) {
            result.issues.push_back(
                {item.metadata.id, SessionDocumentDisposition::rejected, false});
            continue;
        }
        ++result.restored_documents;
    }
    if (session.active && target.Find(*session.active)) target.Activate(*session.active);
    for (const auto& item : session.recently_closed) {
        if (assess(item, true)) recent.push_back(item.metadata);
    }
    if (!recent.empty()) {
        if (target.RestoreRecentlyClosed(recent))
            result.restored_recently_closed = recent.size();
        else
            for (const auto& item : recent)
                result.issues.push_back(
                    {item.id, SessionDocumentDisposition::rejected, true});
    }
    result.status = result.issues.empty() ? SessionRestoreStatus::success
                                          : SessionRestoreStatus::partial_success;
    return result;
}

}  // namespace mwfl
