# PowerNapp in 35 Languages Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** PowerNapp's UI and Store listing in 35 languages, with per-language files, a language picker, Windows regional clock and week, and mirrored Arabic and Hebrew.

**Architecture:** Every UI string gets a stable key in one X-macro table (`src/strings.h`) that holds the built-in English. `src/lang.c` loads `lang/<tag>.txt` (UTF-8 `key = value`) into a string array, falling back per key to English, and resolves the language from the saved setting or the Windows UI languages. RTL is done by creating windows with `WS_EX_LAYOUTRTL` and drawing with `DT_RTLREADING`. Translations are produced by parallel subagents against `lang/en.txt`, gated by a validator and an automatic text-fit check.

**Tech Stack:** C (MSVC, unity build `powernapp.c`), Win32/GDI, NSIS, MSIX (makeappx, makepri), PowerShell sandbox tests.

**Spec:** `docs/superpowers/specs/2026-10-18-localization-design.md`

## Global Constraints

- Scope: the app UI and the Store listing only. Installer, README and CLI output (`--help`, console messages) stay English.
- 35 languages, these tags exactly: en ar bg cs da de el es fi fr he hi hr hu id it ja ko ms nb nl pl pt-BR pt-PT ro ru sk sr-Latn sv th tr uk vi zh-Hans zh-Hant.
- Language files: `lang/<tag>.txt` next to `powernapp.exe`, UTF-8, `key = value`, `#` comments, `\n` escapes, header with the language's own name and `rtl = 1` for ar and he. English is built into the exe; any missing file or key falls back to English.
- The name is "PowerNapp" in every language; the company is "PowerNapps". Neither is translated; URLs are not translated.
- The 12/24-hour clock and the first day of the week follow Windows regional settings in every language, English included. Digits stay Latin.
- Resolution order: saved choice, else Windows preferred UI languages → exact tag → base language → English; `pt-BR` exact, any other `pt*` → `pt-PT`; `zh-CN/SG/Hans*` → `zh-Hans`, `zh-TW/HK/MO/Hant*` → `zh-Hant`; `sr*` → `sr-Latn`; `no`/`nn` → `nb`.
- Nothing clipped in any language at 100% and 150% scaling; ellipsis only on subtitles, never on a control's main label.
- Exe grows < 10 KB; the tray-only process stays ≈ 0.1 MB (Task Manager private working set).
- Message-box buttons are Windows' own; only message text is translated.
- English UI must not change, except for following the regional clock and first day of the week.

## Review Focus

- A translation that drops or reorders a `%ls`/`%d` placeholder must never reach `StringCchPrintfW`: that key falls back to English (Task 1 test `placeholder_mismatch_falls_back`).
- A file saved with a UTF-8 BOM, CRLF line ends, or trailing spaces loads normally (Task 1 test `bom_crlf_whitespace`).
- The portable exe with no `lang` folder runs in English and the picker offers only "Windows default" and English (Task 4 sandbox check `no lang folder`).
- The saved language's file is deleted later: the app falls back to Windows default without errors (Task 4 sandbox check `saved language missing`).
- Switching between LTR and RTL while a flyout, the time picker or About is open closes them first and leaves no stray mirrored window (Task 5 sandbox check `switch to rtl with flyout open`).

---

## File Structure

| File | Responsibility |
|---|---|
| `src/strings.h` (new) | `STRINGS(X)` X-macro: `X(ID, "key", L"English")` for every UI string; included by `lang.c` |
| `src/lang.c` (new) | parse, load, resolve, list languages, `TR()`, `lang_rtl()`; included in `powernapp.c` right after `model.c` |
| `src/locale.c` (new) | regional clock, first day of week, day and AM/PM names for the active language; after `lang.c` |
| `src/*.c` (modify) | every UI literal becomes `TR(L_…)`; layout measures text; RTL flags |
| `lang/*.txt` (new) | `en.txt` (generated) + 34 translations |
| `store/listing/*.txt` (new) | Partner Center listing per language |
| `tools/gen-lang-template.ps1` (new) | writes `lang/en.txt` from `src/strings.h`; `-Check` fails on drift |
| `tools/check-lang.ps1` (new) | validates every `lang/*.txt` against `en.txt` |
| `tests/unit/lang_test.c`, `tests/unit/build.bat` (new) | unit tests for `lang.c` and `locale.c` |
| `tests/sandbox/*` (modify) | picker, RTL, regional, fallback, per-language screenshots |
| `installer/powernapp.nsi`, `tools/make-msix.ps1`, `.github/workflows/build.yml` (modify) | ship `lang`, localized MSIX strings, portable zip |

---

### Task 1: String table and language loader

**Files:**
- Create: `src/strings.h`, `src/lang.c`, `tests/unit/lang_test.c`, `tests/unit/build.bat`
- Modify: `powernapp.c` (include `src/lang.c` after `src/model.c`), `.github/workflows/build.yml` (run unit tests)

**Interfaces:**
- Produces:
  - `typedef enum { #define X(id,k,e) L_##id, STRINGS(X) L_COUNT } StrId;`
  - `const wchar_t *TR(StrId id);` (never NULL)
  - `bool lang_parse(const char *utf8, size_t len, const wchar_t **table /*[L_COUNT]*/, wchar_t *name, size_t nn, bool *rtl);` (fills only keys present and valid)
  - `void lang_load(const wchar_t *tag);` (`L"en"` or unknown → built-in English)
  - `void lang_resolve(const wchar_t *setting, wchar_t *tag, size_t n);` (setting `L""` = Windows default)
  - `typedef struct { wchar_t tag[16], name[48]; bool rtl; } LangInfo;` `int lang_list(LangInfo *out, int max);` (English first, then files in `lang\` sorted by name)
  - `bool lang_rtl(void);` `const wchar_t *lang_tag(void);`
  - INI key `lang` in `[PowerNapp]` holds the saved choice (empty = Windows default).
- File format: lines `key = value`; header keys `@name = Deutsch`, `@rtl = 1`; `#` comment lines; value escapes `\n`, `\\`; key names lowercase dotted (e.g. `action.title`).

- [ ] **Step 1: Write the failing unit tests** in `tests/unit/lang_test.c` (it `#include`s the app's headers and `src/lang.c` directly; a tiny `CHECK(cond)` macro counts failures and `main` returns the count):
  - `parse_basic`: `"@name = Deutsch\naction.title = Aktion\n"` → `table[L_ACTION_TITLE]` is `L"Aktion"`, name `L"Deutsch"`, rtl false.
  - `bom_crlf_whitespace`: same content with `EF BB BF` prefix, `\r\n`, and spaces around `=` and at line end → same result.
  - `escapes`: `x = a\nb` → value contains a real newline; `\\` → one backslash.
  - `missing_key_falls_back`: after `lang_load` of a file lacking `action.title`, `TR(L_ACTION_TITLE)` is the English `L"Action"`.
  - `placeholder_mismatch_falls_back`: a value that drops or adds a `%ls`/`%d` relative to English is ignored (English kept).
  - `garbage_file`: random bytes and invalid UTF-8 → no crash, every `TR()` English.
  - `resolve_rules`: with available tags `{de, pt-BR, pt-PT, zh-Hans, zh-Hant, sr-Latn, nb}`: `de-CH`→`de`, `pt-BR`→`pt-BR`, `pt-AO`→`pt-PT`, `zh-CN`→`zh-Hans`, `zh-HK`→`zh-Hant`, `sr-Cyrl-RS`→`sr-Latn`, `nn-NO`→`nb`, `fa-IR`→`en`; a saved setting `de` wins over Windows. (Expose `lang_match(const wchar_t *pref, const wchar_t **avail, int n, wchar_t *out, size_t on)` for this test; `lang_resolve` uses it.)
- [ ] **Step 2: Run** `tests\unit\build.bat` → fails to compile (`lang.c` missing).
- [ ] **Step 3: Implement `src/strings.h` and `src/lang.c`.** Seed `STRINGS` with the keys the tests use; Task 2 fills the rest. `lang_load` reads the file with `CreateFileW`/`ReadFile` from `<exe dir>\lang\<tag>.txt`, converts with `MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS…)` per line, and keeps values in one heap block (freed on reload). Windows languages come from `GetUserPreferredUILanguages(MUI_LANGUAGE_NAME…)`. Placeholder check: same multiset of `%ls`/`%d`/`%lld` tokens as English.
- [ ] **Step 4: Run** `tests\unit\build.bat` → all checks pass, exit 0. Add it as a CI step after `build.bat`.
- [ ] **Step 5: Commit** `Add string table and language loader`.

### Task 2: Every UI string through TR(), and the English template

**Files:**
- Modify: `src/strings.h`, `src/model.c`, `src/views.c`, `src/window.c`, `src/reminder.c`, `src/about.c`, `src/tray.c`, `src/timepicker.c`, `src/flyout.c`, `src/taskschd.c` (task Description text only)
- Create: `tools/gen-lang-template.ps1`, `lang/en.txt`

**Interfaces:**
- Consumes: Task 1 `TR`, `StrId`.
- Produces: keys for every UI string. Lists become key ranges: `L_TASK_SHUTDOWN … L_TASK_DISPLAY` (index-aligned with `Task`), `L_MODE_DAILY … L_MODE_IDLE` (aligned with `GUI_MODES`), `L_RM_*` for the reminder card's per-task wording. Strings with values use printf placeholders in English (`L"Scheduled for %ls \x00B7 %ls left"`). `TASK_NAMES`/`GUI_MODE_NAMES` arrays are removed; CLI keeps `TASK_CLI` and its own English console text.
- `gen-lang-template.ps1`: parses `X(ID, "key", L"…")` lines → `lang/en.txt` with `@name = English`; `-Check` exits 1 if the file on disk differs.

- [ ] **Step 1:** Add a unit test `every_key_unique`: no two `STRINGS` entries share a key (build a list from the X-macro at runtime). Run → passes trivially now; it guards the bulk edit.
- [ ] **Step 2:** Replace every UI string literal (titles, subtitles, combo items, durations like "1 hour 30 minutes", hero and status lines, accessibility names and descriptions, message boxes, tray menu and tooltip, About text, reminder text) with `TR()`; keep `APP_NAME`, URLs, class names, task names and INI keys as literals. Durations and plurals: one key per form the English uses (`time.minute`, `time.minutes`, `time.hour`, `time.hours`); languages with more plural forms get them in Task 8 via extra keys only if the fit/validator task shows a need (YAGNI otherwise).
- [ ] **Step 3:** Run `tools\gen-lang-template.ps1` → writes `lang\en.txt`; add `-Check` to CI.
- [ ] **Step 4:** Build; run the full sandbox suite (`tests\sandbox\Run-Sandbox.ps1`) → all checks pass with English unchanged (it already asserts English UIA names such as `Remind me 1 minute before`).
- [ ] **Step 5: Commit** `Route every UI string through TR()`.

### Task 3: Regional clock, first day of week, localized day and AM/PM names

**Files:**
- Create: `src/locale.c`
- Modify: `src/model.c` (`fmt12` → `fmt_time`, `days_text`, the "Today/Tomorrow/weekday" text), `src/timepicker.c` (two columns in 24-hour mode), `src/window.c`/`src/layout.c` (day buttons start at the first day), `tests/unit/lang_test.c`, `tests/sandbox/run-tests.ps1`

**Interfaces:**
- Produces: `bool clock_24h(void);` (user's `LOCALE_STIMEFORMAT` contains `H`), `int first_day(void);` (0 = Monday … 6 = Sunday, from `LOCALE_IFIRSTDAYOFWEEK`), `void day_name(int d, bool full, wchar_t *out, size_t n);` and `void ampm(bool pm, wchar_t *out, size_t n);` (from `GetLocaleInfoEx` with `lang_tag()`), `void fmt_time(int h, int m, wchar_t *out, size_t n);` (`23:05` or `11:05 PM`, locale's AM/PM). "Today"/"Tomorrow" are `TR` keys. The `days` bitmask stays Monday-based; only display order rotates.

- [ ] **Step 1:** Unit tests: `fmt_time` with a forced 24-hour flag → `L"23:05"`, with 12-hour and English → `L"11:05 PM"`; `day_name(0, false…)` under `lang_load(L"de")` (test sets the tag) → `L"Mo"` (Windows' German abbreviation).
- [ ] **Step 2:** Sandbox checks (set before starting the app via `Set-ItemProperty 'HKCU:\Control Panel\International'` `sShortTime`/`sTimeFormat` = `HH:mm`, `iFirstDayOfWeek` = `6`): the time button's UIA value is `23:00`; the time picker has no AM/PM column; the first day button's UIA name is Sunday's. Restore the US defaults afterwards.
- [ ] **Step 3:** Implement; run unit tests and the sandbox suite → pass.
- [ ] **Step 4: Commit** `Follow Windows regional clock and first day of week`.

### Task 4: Language picker

**Files:**
- Modify: `src/window.c`, `src/layout.c`, `src/views.c`, `src/model.c` (INI `lang`), `src/cli.c` (tray and `--fire-now` load the language), `tests/sandbox/run-tests.ps1`

**Interfaces:**
- Consumes: `lang_list`, `lang_resolve`, `lang_load`, `TR`.
- Produces: a new last row in Options (after Wake time): icon, title `TR(L_LANGUAGE_TITLE)`, a 200 px dropdown (same flyout as Action) with items "Windows default (<resolved language's own name>)" then each `LangInfo.name`. Control ID `ID_COMBO_LANG = 205`. `void apply_language(void);` re-resolves, loads, rebuilds fonts if needed, re-lays out, resets every combo's text, `acc_sync_all()`, and invalidates; it closes open popups first. A test pseudo-language file `tests/sandbox/qps-ploc.txt` (accented English, +40% length, `@name = Pseudo`) is generated by `tools/gen-lang-template.ps1 -Pseudo`.

- [ ] **Step 1:** Sandbox checks (the runner copies `lang\` and the pseudo file into the install's `lang`): picker UIA value is `Windows default (English)`; choosing `Pseudo` changes the Start button's UIA name to the pseudo string without restarting and writes `lang=qps-ploc` to the INI; restarting keeps it; `no lang folder` (rename it away) → picker lists only Windows default and English; `saved language missing` (delete the file with `lang=qps-ploc` saved) → app starts in English and the picker shows Windows default; the reminder card from `--fire-now --warn-secs 30` shows the pseudo text.
- [ ] **Step 2:** Implement; run → pass.
- [ ] **Step 3: Commit** `Add the language picker`.

### Task 5: Right-to-left (Arabic, Hebrew)

**Files:**
- Modify: `src/window.c` (main window and title bar hit-testing), `src/flyout.c`, `src/timepicker.c`, `src/about.c`, `src/reminder.c` (bottom-left placement), `src/theme.c` (`text_at` adds `DT_RTLREADING`; memory DCs use `SetLayout(hdc, LAYOUT_RTL | LAYOUT_BITMAPORIENTATIONPRESERVED)` so icons are not flipped), `tests/sandbox/run-tests.ps1`

**Interfaces:**
- Consumes: `lang_rtl()`, `apply_language()`.
- Produces: every top-level window created with `WS_EX_LAYOUTRTL` when `lang_rtl()`. Switching between LTR and RTL in `apply_language()` destroys and recreates the main window at the same position (children don't re-mirror in place). Times keep LTR order (draw them with `DT_LTRREADING`-equivalent: no `DT_RTLREADING`).

- [ ] **Step 1:** Sandbox checks with `tests/sandbox/qps-plocm.txt` (`@rtl = 1`, mirrored pseudo strings): the main window has `WS_EX_LAYOUTRTL`; the Start button's screen x is left of the hero text; the minimize/maximize/close buttons are on the left (hit-test `HTCLOSE` near the left edge); the reminder card's right edge is in the left half of the work area; `switch to rtl with flyout open`: open the Action flyout, switch the language via the picker's command, then no `PowerNappFly` window exists and the main window is RTL; a mirrored screenshot is saved for review.
- [ ] **Step 2:** Implement; run → pass. Review the screenshot: icons not mirrored, text right-aligned.
- [ ] **Step 3: Commit** `Mirror the UI for Arabic and Hebrew`.

### Task 6: Text fits in every language

**Files:**
- Modify: `src/theme.c` (`text_at` records overflow in check mode), `src/layout.c` (widths from measured text: combos `max(200, widest item + 48)` up to the row space, Start button `max(120, text + 40)`, toggle state label `max(34, On/Off width)`), `src/cli.c` (`--check-fit <tag>`)

**Interfaces:**
- Produces: `powernapp.exe --check-fit <tag> [--dpi 144]` lays out and paints the main window (every mode and Wake state), every flyout's items, About and the reminder card offscreen, then prints one line per string whose measured width exceeds its rect (`key: needs 212 px, has 180`) and exits with the count (0 = fits). Subtitles may ellipsize and are not reported.

- [ ] **Step 1:** With the pseudo file: `--check-fit qps-ploc` → nonzero before the layout changes (proves the check sees overflow).
- [ ] **Step 2:** Implement measured widths; `--check-fit qps-ploc` and `--check-fit qps-ploc --dpi 144` → 0; `--check-fit en` → 0; the sandbox suite still passes.
- [ ] **Step 3: Commit** `Size controls to their text; add --check-fit`.

### Task 7: Ship the languages

**Files:**
- Modify: `installer/powernapp.nsi` (install and uninstall `$INSTDIR\lang\*.txt`), `tools/make-msix.ps1` (copy `lang`; `strings/<tag>/resources.resw` with `AppDescription` from each `store/listing/<tag>.txt`; manifest `Description="ms-resource:AppDescription"`; one `<Resource Language>` per shipped tag), `.github/workflows/build.yml` (build `PowerNapp-portable.zip` = exe + `lang`; upload and attach it), `tests/sandbox/msix-tests.ps1`, `tests/sandbox/run-tests.ps1`

- [ ] **Step 1:** Sandbox checks: after install `lang\de.txt` exists next to the exe; after uninstall the folder is gone; in the MSIX suite the package's `lang` folder exists and the app shows the pseudo language when `lang=qps-ploc`… (MSIX test copies the pseudo file into the layout before registering).
- [ ] **Step 2:** Implement; run both suites → pass; CI artifact contains `PowerNapp-portable.zip`.
- [ ] **Step 3: Commit** `Ship language files with the installer, Store package and portable zip`.

### Task 8: Translations (parallel subagents)

**Files:**
- Create: `lang/<tag>.txt` for the 34 tags, `tools/check-lang.ps1`

**Interfaces:**
- `tools/check-lang.ps1 [-Tag de]`: for each file, every `en.txt` key present, no unknown keys, same placeholders, `@name` set, `@rtl = 1` exactly for ar and he, valid UTF-8; exits nonzero with a list of problems.

- [ ] **Step 1:** Write `check-lang.ps1`; run it on a copy of `en.txt` renamed `xx.txt` → passes; with one key deleted → fails naming the key.
- [ ] **Step 2:** Dispatch 7 subagents in parallel, 5 languages each (ar he tr hi th / de nl sv da nb / fr es it pt-BR pt-PT / pl cs sk hu ro / bg hr sr-Latn el ru / uk fi ja ko vi / zh-Hans zh-Hant id ms). Each gets: `lang/en.txt`, the spec's terminology rule (Microsoft's own Windows translations for Shut down, Restart, Sleep, Hibernate, Sign out, Lock, Turn off display, On/Off, Start, Cancel, Snooze), the rule that "PowerNapp"/"PowerNapps"/URLs stay, and must make `check-lang.ps1 -Tag <tag>` and `powernapp.exe --check-fit <tag>` (100% and 144 DPI) return 0 before finishing, shortening phrasing rather than truncating.
- [ ] **Step 3:** Run `check-lang.ps1` and `--check-fit` for all 35 → 0 problems.
- [ ] **Step 4: Commit** `Add 34 translations`.

### Task 9: Store listing in 35 languages

**Files:**
- Create: `store/listing/en.txt` and 34 translations, `store/README.md` (how to paste or import into Partner Center)

**Interfaces:**
- Listing file fields, one per header line followed by the text: `# Description`, `# Short description` (≤ 270 chars, also used as the MSIX `AppDescription`), `# Features` (one per line, ≤ 20, each ≤ 200 chars), `# Keywords` (≤ 7, each ≤ 40 chars), `# What's new` (this version). English written from `README.md` (0 MB idea, wake timer, Settings look, 1-minute reminder, reliable via Task Scheduler, languages).

- [ ] **Step 1:** Write `en.txt`; extend `check-lang.ps1 -Listing` to enforce the field set and limits; it passes on `en.txt`.
- [ ] **Step 2:** Same 7 subagent batches translate it (can run alongside Task 8); `check-lang.ps1 -Listing` → 0 for all.
- [ ] **Step 3: Commit** `Add the Store listing in 35 languages`.

### Task 10: Screenshots and visual review

**Files:**
- Modify: `tests/sandbox/screenshots.ps1` (`-Languages all`: for each shipped tag set `lang`, capture the main window, the Action flyout, the reminder card and About; crop the 8 px border), `tests/sandbox/Run-Sandbox.ps1` (pass through `-Languages`)
- Create (not committed): a contact sheet per batch in the results folder

- [ ] **Step 1:** Run `Run-Sandbox.ps1 -Suite screenshots -Languages all` → 140 images.
- [ ] **Step 2:** Dispatch 7 review subagents (same batches) to inspect their languages' images for clipped text, missing glyphs (boxes), wrong direction and leftover English; fix the files and repeat until clean.
- [ ] **Step 3:** Replace `docs/screenshot-light.png`/`-dark.png` with the new English window (it now has the Language row); keep per-language main-window shots as Store screenshots outside the repo (release artifact `store-screenshots.zip`).
- [ ] **Step 4:** Update README (languages, the picker, regional clock) and commit `Screenshots and docs for 35 languages`.
