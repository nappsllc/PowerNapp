"""Fill a Partner Center "Export listings" CSV with PowerNapp's Store listings.

    python tools/make-listing-import.py [--split] <exported.csv> [screenshots.zip] [out-dir]

Reads store/listing/<tag>.txt (sections: # Description, # Short description,
# Features, # Keywords, # What's new) for every language column in the exported
template, fills Title, Description, ReleaseNotes, ShortDescription, DevStudio,
copyright, Feature1-20 and SearchTerm1-7, and points DesktopScreenshot1-3 at
<out-dir name>/<tag>/1.png .. 3.png. The screenshots zip (folders per language, 1.png .. 3.png)
is unpacked next to the CSV, so in Partner Center choose Import listings and
select the whole out-dir folder.

--split writes one import folder per language instead (out-dir/<tag>/ with a
listingData.csv holding only that language's column, plus 1.png .. 3.png), for
when a single import of every language is too big for Partner Center.
"""
import csv, os, re, sys, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TITLE = 'PowerNapps PowerNapp'            # the name reserved in Partner Center
DEV = 'PowerNapps'
COPYRIGHT = '© 2026 PowerNapps (by Denis Platonov)'
SHOTS = 3


def read_listing(path):
    sections, cur = {}, None
    for line in open(path, encoding='utf-8-sig').read().splitlines():
        m = re.match(r'^# (.+?)\s*$', line)
        if m:
            cur = m.group(1).lower()
            sections[cur] = []
        elif cur is not None:
            sections[cur].append(line)
    text = lambda k: '\n'.join(sections.get(k, [])).strip()
    lines = lambda k: [l.strip() for l in sections.get(k, []) if l.strip()]
    return {
        'Description': text('description'),
        'ShortDescription': text('short description'),
        'ReleaseNotes': text("what's new"),
        'features': lines('features'),
        'keywords': lines('keywords'),
    }


def main():
    args = sys.argv[1:]
    split = '--split' in args
    args = [a for a in args if a != '--split']
    if not args:
        sys.exit(__doc__)
    template = args[0]
    shots_zip = args[1] if len(args) > 1 else os.path.join(ROOT, 'store-upload', 'PowerNapp-store-screenshots.zip')
    out_dir = args[2] if len(args) > 2 else os.path.join(ROOT, 'store-upload', 'listings-split' if split else 'listings')

    rows = list(csv.reader(open(template, encoding='utf-8-sig', newline='')))
    header = rows[0]
    langs = header[4:]
    files = {f[:-4].lower(): os.path.join(ROOT, 'store', 'listing', f)
             for f in os.listdir(os.path.join(ROOT, 'store', 'listing')) if f.endswith('.txt')}
    missing = [t for t in langs if t not in files]
    if missing:
        sys.exit('no store/listing file for: ' + ', '.join(missing))
    listings = {t: read_listing(files[t]) for t in langs}

    os.makedirs(out_dir, exist_ok=True)
    have_shots = set()
    if os.path.exists(shots_zip):
        with zipfile.ZipFile(shots_zip) as z:
            for name in z.namelist():
                m = re.match(r'^([^/]+)/(\d+)\.png$', name)
                if not m or m.group(1).lower() not in langs or int(m.group(2)) > SHOTS:
                    continue
                tag = m.group(1).lower()
                os.makedirs(os.path.join(out_dir, tag), exist_ok=True)
                with open(os.path.join(out_dir, tag, m.group(2) + '.png'), 'wb') as f:
                    f.write(z.read(name))
                have_shots.add((tag, int(m.group(2))))

    folder = os.path.basename(os.path.normpath(out_dir))
    for r in rows[1:]:
        if not r or not r[0]:
            continue
        field = r[0]
        for i, tag in enumerate(langs):
            L, col, val = listings[tag], 4 + i, None
            if field in ('Description', 'ShortDescription', 'ReleaseNotes'):
                val = L[field]
            elif field == 'Title':
                val = TITLE
            elif field == 'DevStudio':
                val = DEV
            elif field == 'CopyrightTrademarkInformation':
                val = COPYRIGHT
            elif (m := re.match(r'^Feature(\d+)$', field)):
                n = int(m.group(1))
                val = L['features'][n - 1] if n <= len(L['features']) else ''
            elif (m := re.match(r'^SearchTerm(\d+)$', field)):
                n = int(m.group(1))
                val = L['keywords'][n - 1] if n <= len(L['keywords']) else ''
            elif (m := re.match(r'^DesktopScreenshot(\d+)$', field)):
                n = int(m.group(1))
                # Partner Center wants the path from the imported folder itself: <folder>/<tag>/<n>.png
                path = f'{tag}/{n}.png' if split else f'{folder}/{tag}/{n}.png'
                val = path if (tag, n) in have_shots else ''
            if val is not None:
                while len(r) <= col:
                    r.append('')
                r[col] = val

    if split:
        # one folder per language: its images sit next to its own CSV
        for i, tag in enumerate(langs):
            col = 4 + i
            os.makedirs(os.path.join(out_dir, tag), exist_ok=True)
            sub = [r[:4] + [r[col] if len(r) > col else ''] for r in rows]
            with open(os.path.join(out_dir, tag, 'listingData.csv'), 'w', encoding='utf-8-sig', newline='') as f:
                csv.writer(f).writerows(sub)
        print(f'{out_dir}: {len(langs)} folders, {len(have_shots)} screenshots')
        return
    out_csv = os.path.join(out_dir, 'listingData.csv')
    with open(out_csv, 'w', encoding='utf-8-sig', newline='') as f:
        csv.writer(f).writerows(rows)
    print(f'{out_csv}: {len(langs)} languages, {len(have_shots)} screenshots')


if __name__ == '__main__':
    main()
