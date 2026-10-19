# Microsoft Store listing

`listing\<tag>.txt` holds the Store listing for one language (the tag is the
Store language, e.g. `en`, `de`, `pt-BR`, `zh-Hans`). Each file has five
sections, each started by a header line:

| Header | Limit | Partner Center field |
|---|---|---|
| `# Description` | none | Description |
| `# Short description` | 270 chars | Short description (also the MSIX `AppDescription`, written by `tools\make-msix.ps1`) |
| `# Features` | 20 lines, 200 chars each | Product features (one per line) |
| `# Keywords` | 7 lines, 40 chars each | Search terms (one per line) |
| `# What's new` | none | What's new in this version |

Check them (CI does too):

    powershell -File tools\check-lang.ps1 -Listing

## Pasting into Partner Center

1. In Partner Center open the app, Product setup, then the submission's
   **Store listings**. Add a listing for each language (the Store shows
   the languages that `make-msix.ps1` declared from `listing\*.txt`).
2. In each language, paste the matching sections of `listing\<tag>.txt`
   into the fields in the table above. Product name is `PowerNapp` in every
   language.
3. Update **What's new** for every release; the other sections change only
   when the app does.

## Package

`PowerNapp.msix` attached to each GitHub release is the package to upload
(Packages step of the submission). It is unsigned; the Store signs it.

## Screenshots

Store screenshots come from the sandbox suite:

    powershell -File tests\sandbox\Run-Sandbox.ps1 -Suite screenshots
