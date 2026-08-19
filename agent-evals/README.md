# mwfl coding-agent evaluations

This suite measures whether a coding agent can turn a natural-language Windows
UI request into correct, compilable mwfl code without inventing APIs or
violating lifecycle rules.

`tasks.json` contains thirty-four prompts and their required evidence. `fixtures/`
contains minimal known-good public-API baselines compiled by the repository.
The baselines are not supplied to an agent during a blind run. `rubric.json`
defines a 100-point score and `score-result.ps1` calculates it from recorded
evidence.

## Run a blind evaluation

1. Give the agent one prompt from `tasks.json`, plus only the public repository
   context a normal user would provide.
2. Save its single translation unit outside `fixtures/`.
3. Compile, record conservative evidence, and score it with:

```powershell
./agent-evals/run-eval.ps1 -TaskId 01-window -Candidate path/to/main.cpp -Agent codex -Model model-name
```

4. Review the generated evidence JSON, then rescore it if a human check changes
   semantic evidence:

```powershell
./agent-evals/score-result.ps1 -Result path/to/result.json
```

Track first-attempt compilation separately from eventual compilation. Do not
edit an agent result before recording first-attempt evidence.

## Run the complete blind baseline

Prepare prompt-only inputs outside the repository, give each prompt to the
same clean agent/model configuration, and save each answer as `<task-id>.cpp`:

```powershell
./agent-evals/prepare-blind-run.ps1 -OutputDirectory ../mwfl-blind-run
./agent-evals/run-suite.ps1 -CandidateDirectory ../mwfl-blind-run/candidates `
  -Agent codex -Model model-name -Blind
```

`run-suite.ps1` refuses to treat the checked-in golden fixtures as blind,
records all 34 first-attempt results, and enforces `baseline-policy.json`.
Only an actually isolated run may be checked in as a blind baseline.

The fixture matching each task is the golden public-API patch. It is compiled
with `/W4 /permissive- /WX`; it is a structural reference, not a byte-for-byte
answer requirement.

Checked-in results under `results/` state whether a run was blind or performed
inside the implementation task. `11-notepad-setting-codex.json` records the
current in-task Codex run and its first-compile evidence; it does not claim
independent blind-run evidence.

## Verify the suite

The normal repository build compiles the core fixtures and the printing, OLE,
Shell, Ribbon, MDI, and graphics fixtures. A build with both third-party
components enabled also compiles the WebView2 and Scintilla integration fixtures. Asset consistency
is checked by `mwfl.agent_evals` and by:

```powershell
./scripts/verify.ps1 -Mode Docs
```
