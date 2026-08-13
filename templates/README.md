# Application templates

- `basic-app`: smallest recommended native window skeleton.
- `form-app`: explicit model, validation, focus recovery, and responsive form.

Copy a directory into a new project. The default consumption path pins mwfl to
the `v0.1.3` release; set `MWFL_SOURCE_DIR` to compile against a local checkout.

Create a starter without copying files by hand:

```powershell
./scripts/new-mwfl-app.ps1 -Name MyApp -Destination D:\Projects
```

Use `-Template form` for the form/binding starter. The generator refuses to
overwrite a non-empty destination.
