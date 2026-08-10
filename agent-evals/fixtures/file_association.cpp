#include <mwfl/file_association.h>

mwfl::FileAssociationResult RegisterFixtureAssociation(
    const std::filesystem::path& executable) {
    mwfl::FileAssociationSpec spec{
        .extension = L".mwflfixture",
        .prog_id = L"mwfl.agent.fixture.document",
        .owner_id = L"mwfl.agent.fixture",
        .display_name = L"mwfl Agent fixture",
        .executable = executable,
        .icon = executable,
        .verbs = {{L"open", L"Open", {}}}};
    return mwfl::RegisterPerUserFileAssociation(spec);
}
