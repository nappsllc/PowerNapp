# Microsoft Store submission: PowerNapp

A step-by-step checklist for the first Partner Center submission, with every
field filled in from this repo. Items marked **TODO (you)** need the account
owner; everything else is ready to paste.

Order of the steps below follows the submission page in Partner Center
(Apps and games > PowerNapp > the submission).

## 0. Before the submission

- [ ] **TODO (you):** Partner Center developer account (individual or company).
      The account's publisher display name becomes `PublisherDisplayName`.
- [ ] **TODO (you):** Reserve the product name **PowerNapp** (Apps and games >
      New product > MSIX or PWA app).
- [ ] **TODO (you):** Deploy https://powernapps.net/privacy (the privacy policy
      page is still pending deploy; the submission needs a live URL).
- [ ] **TODO (you):** Copy the package identity into the repo and build the
      release package (section 4).

## 1. Pricing and availability

| Field | Value |
|---|---|
| Markets | All markets (default: "Make available in all markets", including future ones) |
| Discoverability | Make this product available and discoverable in the Microsoft Store |
| Schedule | As soon as possible after certification (or "Manually" if you want to press Publish yourself) |
| Pricing | **Free** |
| Free trial | No free trial |
| Sale pricing | none |
| Organizational licensing | leave the defaults (allow Store for Business / Education) |

## 2. Properties

| Field | Value |
|---|---|
| Category | **Utilities & tools** |
| Subcategory | none needed (pick the closest one only if Partner Center requires it) |
| Privacy policy URL | `https://powernapps.net/privacy` (**TODO (you):** deploy first) |
| Website | `https://powernapps.net` |
| Support contact info | `hi@powernapps.net` |
| Game settings | not a game: leave empty |
| Product declarations | Check "This app has been tested to meet accessibility guidelines" (keyboard, Narrator and per-monitor DPI are tested by the sandbox suite). Leave "depends on non-Microsoft drivers or NT services" unchecked. Leave "customers can install to alternate drives" and "OneDrive backup" at their defaults. |
| System requirements: Minimum hardware | none required. Input: Keyboard and Mouse (optional to tick; the app works with either, and with touch) |
| System requirements: OS | comes from the package: Windows 10 version 1809 (build **17763**) or later, Windows 11; **x64** only (`TargetDeviceFamily Windows.Desktop MinVersion=10.0.17763.0`, `ProcessorArchitecture=x64`) |

PowerNapp collects no personal data and makes no network requests; the
privacy policy should say so. The About dialog's "Visit our website" and
contact buttons only open the default browser or mail app.

## 3. Age ratings (IARC questionnaire)

Category: **Utility, Productivity, Communication, or Other**.

| Question | Answer |
|---|---|
| Violence, fear, sexuality, gambling, language, controlled substances, crude humor | No (none of it) |
| Does the app let users interact or exchange content with each other (user-generated content, chat)? | **No** |
| Does the app share the user's current physical location with other users? | **No** |
| Does the app allow purchases of digital goods (in-app purchases)? | **No** |
| Does the app collect, share or transmit personal data? | **No** (no data collection, no network access) |
| Does the app provide unrestricted internet access (a browser)? | **No** (links only open the user's own browser) |

Expected result: the lowest rating in every system (IARC 3+, ESRB Everyone, PEGI 3).

## 4. Packages

### 4a. Package identity (one time)

In Partner Center: **Product management > Product identity**. Copy these three
values into the GitHub repository variables (Settings > Secrets and variables >
Actions > Variables), or with `gh`:

| Partner Center field | Repo variable | Example shape |
|---|---|---|
| Package/Identity/Name | `MSIX_NAME` | `12345PowerNapps.PowerNapp` |
| Package/Identity/Publisher | `MSIX_PUBLISHER` | `CN=XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX` |
| Package/Properties/PublisherDisplayName | `MSIX_PUBLISHER_NAME` | `PowerNapps` |

    gh variable set MSIX_NAME --body "<Package/Identity/Name>"
    gh variable set MSIX_PUBLISHER --body "CN=<...>"
    gh variable set MSIX_PUBLISHER_NAME --body "<PublisherDisplayName>"

- [ ] **TODO (you):** set the three variables. Until they are set, CI builds
      `PowerNapp.msix` with placeholder identity (`PowerNapps.PowerNapp`,
      `CN=PowerNapps`), which Partner Center rejects.

### 4b. Version rules

- The package version has four parts and the last one must be `0`: the
  first Store package is **2.0.0.0** (the `make-msix.ps1` default; the desktop
  releases were PowerOff 1.0.x).
- A tag `vX.Y.Z` makes CI build the package as `X.Y.Z.0` (and the installer as
  `X.Y.Z`). Use plain numeric tags only (no `-beta` suffix).
- Every later submission needs a higher version than the last one uploaded.

### 4c. Build the release package

    git tag v2.0.0
    git push origin v2.0.0

CI (`.github/workflows/build.yml`) builds `PowerNapp.msix` with the identity
from the repo variables and version 2.0.0.0, and attaches it (with the
installer, portable zip and exe) to the GitHub release `v2.0.0`. Without a tag,
`gh workflow run build.yml --ref main` builds the same package as version
2.0.0.0 as a run artifact (`gh run download <run id> --name PowerNapp`).

The package is unsigned: the Store signs it. Check it before uploading
(identity values in the manifest):

    tar -xOf PowerNapp.msix AppxManifest.xml | findstr "Identity PublisherDisplayName"

### 4d. Upload

- [ ] Drag `PowerNapp.msix` into the Packages page. Device families: **Windows
      10/11 Desktop** only (leave the other families unchecked).
- [ ] Restricted capability `runFullTrust`: Partner Center asks why the app
      needs it. Paste:

      > Schedules shutdown, restart and sleep with Windows Task Scheduler.
      > PowerNapp is a desktop (Win32) app: it registers Task Scheduler tasks
      > that run its app execution alias and calls the Windows power APIs,
      > which need a full-trust process.

## 5. Store listings (35 languages)

The listing text for each language is in `store/listing/<tag>.txt`: `ar bg cs
da de el en es fi fr he hi hr hu id it ja ko ms nb nl pl pt-BR pt-PT ro ru sk
sr-Latn sv th tr uk vi zh-Hans zh-Hant`. The package declares the same 35
languages (`make-msix.ps1` writes one `<Resource Language>` per listing), so
Partner Center offers a listing for each after the package upload.

For each language (Store listings > the language > Edit):

| Partner Center field | Value |
|---|---|
| Product name | `PowerNapp` (same in every language) |
| Description | `# Description` section of `<tag>.txt` |
| What's new in this version | `# What's new` section |
| Product features | `# Features` section, one line per feature box (up to 20) |
| Short description | `# Short description` section (270 chars max; also the package description) |
| Screenshots | `store-shots/<tag>/1.png`, `2.png`, `3.png` (section 6) |
| Store logos | optional (**TODO (you)** if wanted: 1:1 box art 1080x1080 / 2:3 poster 720x1080; the package's own logos are used otherwise) |
| Trailers | none |
| Search terms (keywords) | `# Keywords` section, one per box (7 max, 40 chars each) |
| Copyright and trademark info | `© 2026 PowerNapps (by Denis Platonov)` |
| Additional license terms | optional: `Free and open source under GPL-3.0: https://github.com/nappsllc/PowerNapp` |
| Developed by | `PowerNapps` |
| Applicable license terms | Microsoft Standard Application License Terms (default) |

Check the files before pasting (CI runs this too):

    powershell -File tools\check-lang.ps1 -Listing

Tip: with 35 languages, Partner Center's **Import listings** (export a CSV
of the empty listings, fill it, import) is faster than pasting by hand.

## 6. Screenshots

Three 1920x1080 PNGs per language (Partner Center accepts desktop
screenshots from 1366x768 up, up to 10 per language):

1. `1.png` the main window (Sleep at 11 PM, Wake up at 7:00)
2. `2.png` the main window with the Action dropdown open
3. `3.png` the 1-minute reminder card

`en-dark/` holds the English set on a dark background (optional extra for the
English listing).

The current set (35 languages + en-dark, 108 PNGs) is in the git-ignored
workspace: `.superpowers\sdd\2026-10-18-localization\store-shots.zip`
(unpacked in `store-shots\`). To regenerate it:

    powershell -File tests\sandbox\Run-Sandbox.ps1 -Suite screenshots -Languages all -NativeRegion
    python -I .superpowers\sdd\2026-10-18-localization\store-shots.py %TEMP%\powernapp-sandbox\results <out dir>

The first command captures each language in Windows Sandbox in its country's
regional format (24-hour clock, first day of week); the second composes each
capture centered on a soft Windows 11 style gradient at 1920x1080.

## 7. Submission options

| Field | Value |
|---|---|
| Publishing hold options | Publish as soon as certified (or manually) |
| Notes for certification | see below |
| Restricted capabilities | runFullTrust justification from 4d |

Notes for certification (paste):

> PowerNapp schedules power actions with Windows Task Scheduler. To test
> safely, choose the action "Lock" or "Turn off display" with a short
> countdown, or press Cancel after Start; Shut down and Restart really
> shut down or restart the test machine when the time comes. No account,
> sign-in or network connection is needed.

## 8. After submitting

- [ ] Certification usually takes up to 3 business days.
- [ ] For each later release: bump the tag (`v2.0.1` -> package 2.0.1.0),
      update every listing's `# What's new`, upload the new msix.

## Still TODO for you (summary)

1. Partner Center account and the reserved name PowerNapp.
2. Deploy https://powernapps.net/privacy.
3. Set `MSIX_NAME`, `MSIX_PUBLISHER`, `MSIX_PUBLISHER_NAME` repo variables
   from Product identity.
4. Push tag `v2.0.0`, download `PowerNapp.msix` from the release, upload it.
5. Answer the IARC questionnaire (answers above) and paste the listings.
6. Optional: Store logos / box art.
