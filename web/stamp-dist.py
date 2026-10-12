#!/usr/bin/env python3
"""Give web/dist versioned asset URLs so browsers never mix old and new files.

The page is served without Cache-Control, so a browser may keep an old app.js
for hours after index.html changes. This stamps a content hash onto the URLs
that index.html and app.js use; app.js reads the stamp from its own URL and
appends it to the files it loads at run time (decks, catalog, WebAssembly).
Run after the files are copied into web/dist: python web/stamp-dist.py
"""
import hashlib
import pathlib
import sys

dist = pathlib.Path(__file__).resolve().parent / "dist"
files = sorted(p for p in dist.rglob("*") if p.is_file() and p.name != "index.html")
digest = hashlib.sha256()
for path in files:
    digest.update(path.relative_to(dist).as_posix().encode())
    digest.update(path.read_bytes())
version = digest.hexdigest()[:10]


def stamp(name, pairs):
    path = dist / name
    text = path.read_text(encoding="utf-8")
    for old, new in pairs:
        if text.count(old) != 1:
            sys.exit(f"stamp-dist: expected one '{old}' in {name}, found {text.count(old)}")
        text = text.replace(old, new)
    path.write_text(text, encoding="utf-8")


stamp("index.html", [('href="./style.css"', f'href="./style.css?v={version}"'),
                     ('src="./app.js"', f'src="./app.js?v={version}"')])
stamp("app.js", [("from './upp.mjs'", f"from './upp.mjs?v={version}'")])
print(f"web/dist stamped with version {version}")
