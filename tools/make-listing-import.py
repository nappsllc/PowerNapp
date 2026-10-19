"""Fill a Partner Center "Export listings" CSV with PowerNapp's Store listings.

    python tools/make-listing-import.py <exported.csv> [screenshots.zip] [out-dir]

Reads store/listing/<tag>.txt (sections: # Description, # Short description,
# Features, # Keywords, # What's new) for every language column in the exported
template, fills Title, Description, ReleaseNotes, ShortDescription, DevStudio,
copyright, Feature1-20 and SearchTerm1-7, and points DesktopScreenshot1-3 at
<tag>/1.png .. 3.png. The screenshots zip (folders per language, 1.png .. 3.png)
is unpacked next to the CSV, so in Partner Center choose Import listings and
select the whole out-dir folder.
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
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    template = sys.argv[1]
    shots_zip = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, 'store-upload', 'PowerNapp-store-screenshots.zip')
    out_dir = sys.argv[3] if len(sys.argv) > 3 else os.path.join(ROOT, 'store-upload', 'listings')

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
                val = f'{tag}/{n}.png' if (tag, n) in have_shots else ''
            if val is not None:
                while len(r) <= col:
                    r.append('')
                r[col] = val

    out_csv = os.path.join(out_dir, 'listingData.csv')
    with open(out_csv, 'w', encoding='utf-8-sig', newline='') as f:
        csv.writer(f).writerows(rows)
    print(f'{out_csv}: {len(langs)} languages, {len(have_shots)} screenshots')


if __name__ == '__main__':
    main()
