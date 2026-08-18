#include <mwfl/security.h>

int main() {
    if (mwfl::UnprotectForCurrentUser({})) return 1;
    std::vector<std::byte> bytes{std::byte{1}, std::byte{2}};
    mwfl::SecureBytes secure{std::span<const std::byte>(bytes)};
    if (secure.Size() != 2) return 2;
    secure.Clear();
    if (secure.Size() != 0) return 3;
    auto protected_data = mwfl::ProtectForCurrentUser(bytes, L"purpose-a");
    if (!protected_data) return 4;
    if (mwfl::UnprotectForCurrentUser(protected_data.Value(), L"purpose-b")) return 5;
    auto restored = mwfl::UnprotectForCurrentUser(protected_data.Value(), L"purpose-a");
    if (!restored || restored.Value().Size() != bytes.size()) return 6;
    auto identity = mwfl::QueryCurrentProcessIdentity();
    if (!identity || identity.Value().user_sid.empty()) return 7;
    auto descriptor =
        mwfl::SecurityDescriptorBuilder{}
            .Owner(identity.Value().user_sid)
            .Rule({mwfl::AccessRuleKind::Allow, identity.Value().user_sid, GENERIC_READ,
                   static_cast<BYTE>(OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE)})
            .Build();
    if (!descriptor || descriptor.Value().empty()) return 8;
    BOOL present = FALSE, defaulted = FALSE;
    PACL dacl = nullptr;
    if (!GetSecurityDescriptorDacl(descriptor.Value().data(), &present, &dacl, &defaulted) ||
        !present || !dacl || dacl->AceCount != 1)
        return 9;
    void* ace = nullptr;
    if (!GetAce(dacl, 0, &ace)) return 10;
    const auto flags = static_cast<ACE_HEADER*>(ace)->AceFlags;
    return (flags & OBJECT_INHERIT_ACE) != 0 && (flags & CONTAINER_INHERIT_ACE) != 0 ? 0 : 11;
}
