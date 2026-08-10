#include <mwtl/file_association.h>

mwtl::FileAssociationResult RegisterFixtureAssociation(
    const std::filesystem::path& executable) {
    mwtl::FileAssociationSpec spec{
        .extension = L".mwtlfixture",
        .prog_id = L"mwtl.agent.fixture.document",
        .owner_id = L"mwtl.agent.fixture",
        .display_name = L"mwtl Agent fixture",
        .executable = executable,
        .icon = executable,
        .verbs = {{L"open", L"Open", {}}}};
    return mwtl::RegisterPerUserFileAssociation(spec);
}
