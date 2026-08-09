# mwtl coding-agent evaluations

This suite measures whether a coding agent can turn a natural-language Windows
UI request into correct, compilable mwtl code without inventing APIs or
violating lifecycle rules.

`tasks.json` contains twelve prompts and their required evidence. `fixtures/`
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

The fixture matching each task is the golden public-API patch. It is compiled
with `/W4 /permissive- /WX`; it is a structural reference, not a byte-for-byte
answer requirement.

Checked-in results under `results/` state whether a run was blind or performed
inside the implementation task. `11-notepad-setting-codex.json` records the
current in-task Codex run and its first-compile evidence; it does not claim
independent blind-run evidence.

## Verify the suite

The normal repository build compiles all twelve fixture sources. Asset consistency
is checked by `mwtl.agent_evals` and by:

```powershell
./scripts/verify.ps1 -Mode Docs
```
