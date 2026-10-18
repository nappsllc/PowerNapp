# PowerNapp in 35 languages: design

## Intent

PowerNapp is meant to feel like part of Windows Settings. For people whose
Windows isn't in English, that only holds if the app speaks their language,
using the same words Windows itself uses ("Sleep", "Hibernate", "Sign out").
It also makes the Microsoft Store listing reach people in their own language.

## Scope

**Translated:**

- **The app:** main window, dropdown and time picker flyouts, reminder card,
  About dialog, tray icon tooltip and menu, error and warning message boxes,
  and every screen-reader name, value and description.
- **The Store listing:** the package's display name and description in the
  MSIX manifest, and the Partner Center listing text (description, short
  description, feature list, keywords, release notes).

**Not translated** (the user decided): the installer and the README.

**Not translated** (builder's default, could be revisited): command-line output
and `--help`. Scripts and logs depend on stable English text.

## Languages (35)

English plus 34, picked as the most-used Windows display languages (the
user asked for the most-used ones, including Arabic and Hebrew):

| Tag | Language | | Tag | Language |
|---|---|---|---|---|
| en | English | | ko | Korean |
| ar | Arabic (RTL) | | ms | Malay |
| bg | Bulgarian | | nb | Norwegian Bokmål |
| cs | Czech | | nl | Dutch |
| da | Danish | | pl | Polish |
| de | German | | pt-BR | Portuguese (Brazil) |
| el | Greek | | pt-PT | Portuguese (Portugal) |
| es | Spanish | | ro | Romanian |
| fi | Finnish | | ru | Russian |
| fr | French | | sk | Slovak |
| he | Hebrew (RTL) | | sr-Latn | Serbian (Latin) |
| hi | Hindi | | sv | Swedish |
| hr | Croatian | | th | Thai |
| hu | Hungarian | | tr | Turkish |
| id | Indonesian | | uk | Ukrainian |
| it | Italian | | vi | Vietnamese |
| ja | Japanese | | zh-Hans | Chinese (Simplified) |
| | | | zh-Hant | Chinese (Traditional) |

## Separate files (the user decided)

Each language is its own file, never compiled into the exe, so the exe stays
tiny and a translation can be added or fixed without touching code.

- **Adding a language is just a file** (builder's call): the picker lists
  whatever valid files are in `lang`, reading each one's header, so the 35
  are the shipped set, not a hard-coded list.
- **App:** `lang/<tag>.txt` next to `powernapp.exe`, for example
  `lang/de.txt`. UTF-8, one `key = value` per line, `#` comments, `\n` for
  line breaks. A header gives the language's own name ("Deutsch", "日本語")
  and its direction (`rtl = 1` for Arabic and Hebrew). Keys are stable
  identifiers, not English text, so rewording English never orphans a
  translation. The builder chooses the key names and the exact syntax.
- **English is built into the exe** and is the fallback for a missing file
  or a missing key, so the app is never blank or half-empty. An English
  `lang/en.txt` is also kept in the repo as the template translators copy;
  it must not drift from the built-in strings (a check or generator is the
  builder's call).
- **Store listing:** `store/listing/<tag>.txt`, one per language, with the
  Partner Center fields. No English listing exists yet: write it from the
  README, then translate it (builder's call). Release notes describe the
  version being submitted. Store screenshots are produced in every language,
  reusing the per-language screenshots from verification (builder's call). The MSIX gets per-language `strings/<tag>/resources.resw`
  for its display name and description, and lists all 35 languages in its
  manifest so the Store shows them as supported.
- The installer and the Store package ship the `lang` folder with the exe.
  The portable `powernapp.exe` release works without it (English only), and
  the release should also offer the exe plus `lang` as a zip.

## Choosing the language

- **Default:** follow the Windows display language. Resolve the user's
  preferred UI languages in order: exact tag, then base language (Swiss
  German uses German, Mexican Spanish uses Spanish, Brazilian Portuguese
  maps to `pt-BR`, any other Portuguese to `pt-PT`), then English.
- **Language picker (the user asked for one):** a new row in Options,
  "Language", with a dropdown. The first entry is "Windows default" showing
  which language that currently resolves to; the rest list every language
  by its own name ("Deutsch", "العربية"), sorted alphabetically. The choice is
  saved in the settings file. Picking a language switches the window
  immediately, without a restart.
- The reminder card shown by Task Scheduler (`--fire-now`) and the tray
  process use the same saved choice.

## Dates, times and days

- **Regional settings win, in every language including English** (the user
  decided): the 12- or 24-hour clock and the first day of the week follow the
  Windows regional format, not the language. So an English user in the UK sees
  a 24-hour clock, and a US user sees "Repeat on" starting on Sunday. In
  24-hour mode the time picker has two columns and no AM/PM.
- AM/PM markers, day names ("Mon", "Monday"), and "Today"/"Tomorrow" come in
  the chosen language. Day and AM/PM names can come from Windows' locale data
  for the chosen language rather than the translation files; builder's call.
- Digits stay Latin (0–9) in every language, as Windows does by default.

## Right-to-left (Arabic, Hebrew)

The whole window mirrors: icons on the right, toggles and dropdowns on the
left, title bar caption buttons on the left, the reminder card in the bottom
left. Text reads right to left. Icons themselves are not flipped, and times
and numbers keep their left-to-right order. Mirroring the device context
(Win32's RTL layout) is the expected approach, but the builder decides how.

## Constraints and quality bar

- **Nothing gets cut off.** German, Finnish and others run 30–50% longer than
  English. Every string must fit in every language at 100% and 150% scaling.
  Prefer fitting by layout (wrap or measure) and by choosing a shorter natural
  phrasing; an ellipsis is a last resort and never on a control's main label.
- **Fonts:** keep Segoe UI Variable; Windows' font fallback supplies Chinese,
  Japanese, Korean, Thai, Hindi, Arabic and Hebrew glyphs. Verify each script
  actually renders (no boxes).
- **Size:** the exe grows only by the loader and the picker (aim: under
  10 KB). Each language file is a few KB.
- **Memory:** the tray-only process must stay at about 0.1 MB, so it loads
  only the few strings it needs, or loads them lazily.
- **Terminology:** use Microsoft's own Windows translations for power
  actions and common UI words in each language.
- **The name stays "PowerNapp" in every language**, including the
  Store display name (builder's call), so only one name is reserved in
  Partner Center.
- **Message boxes:** their buttons come from Windows in the Windows display
  language; only the message text is translated and set right to left for
  Arabic and Hebrew (builder's call).
- **Translation quality:** Claude writes the translations; no native-speaker
  review is planned. Files must be easy for a native speaker to correct later.

## Goals

- All 35 languages complete: every visible string and screen-reader string
  translated.
- Arabic and Hebrew fully mirrored and natural to use.
- The picker works, persists, and applies immediately.
- The Store package and listing are ready for all 35 languages.

## Anti-goals (failures even if it "works")

- Clipped or overlapping text in any language.
- Any untranslated English left in a translated UI (except the app's name
  "PowerNapp", the company name "PowerNapps", and URLs).
- A blank or broken window when a language file is missing or malformed.
- A noticeably bigger exe or a heavier tray process.
- English UI changing in any visible way, other than following the regional
  clock and first day of the week.

## Verification

- Extend the sandbox suite: switching languages via the picker, falling back
  when a file is missing, a garbage file, RTL mirroring, and the reminder card
  in the chosen language.
- Screenshots of the main window, a flyout, the reminder card and About in
  every language, reviewed for clipping and missing glyphs (a contact sheet is
  fine).

## Left to the builder

File syntax and key names, how English stays in sync with `en.txt`, the RTL
technique, where day/AM-PM names come from, how the tray process loads
strings, and the exact Store listing file layout.
