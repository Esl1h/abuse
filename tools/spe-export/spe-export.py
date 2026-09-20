#!/usr/bin/env python3
"""Export the art out of .spe files as PNG, with an index and a licence check.

Phase 6.6 wants an HD pack: sprites upscaled and retouched, dropped in as
`data/hd/<id>.png` with the original as the fallback. Before any of that can
happen the art has to come out, and it has to come out with two things
attached: a name that survives the round trip, so an override can be matched
back to what it replaces, and a licence, so nobody retouches something they
are not allowed to modify.

`abuse-tool getpcx` already decodes one entry. This does the whole tree, adds
the index, and refuses what it must:

    ./tools/spe-export/spe-export.py --out build/export
    ./tools/spe-export/spe-export.py data/art/fore/techno.spe --out /tmp/x

Rule 1 of AGENTS.md is that the original data is never modified, and rule 3
is that every asset carries its licence. Both are enforced here rather than
remembered: anything under classic/ is refused outright, and anything whose
manifest entry does not allow modification is skipped and reported.
"""

import argparse
import json
import pathlib
import re
import subprocess
import sys

TOOL = "build/dev/src/abuse-tool"

# The entry types that hold a picture. Names from src/imlib/specs.h.
IMAGE_TYPES = {
    4: "image",
    5: "foretile",
    6: "backtile",
    7: "character",
    21: "character2",
}

# Not a picture, whatever its name suggests: SPEC_PARTICLE is a list of
# coloured points, and asking abuse-tool for it as PCX yields a truncated
# file. Fifteen of them in lavap.spe, and exporting them produced fifteen
# broken images rather than an error.
NOT_PICTURES = {22: "particle"}

# Licences under which retouching the art is allowed. Everything the
# repository ships is one of these; the list is here so that adding a
# no-derivatives asset later fails loudly instead of silently shipping a
# modified copy of it.
MAY_MODIFY = {"public-domain", "CC0", "CC-BY-4.0", "CC-BY-SA-4.0",
              "GPL-2.0", "GPL-2.0-or-later", "OFL-1.1"}


def run(*args, check=True):
    out = subprocess.run(args, capture_output=True)
    if check and out.returncode != 0:
        sys.exit(f"{' '.join(args)}\n{out.stderr.decode(errors='replace')}")
    return out.stdout


def load_manifest(path="data/MANIFEST.toml"):
    """Licence per path, from the manifest, without a TOML parser.

    The file is simple and stable, and the alternative is a dependency for
    a tool that already shells out to abuse-tool.
    """
    exact, globs = {}, []
    entry = {}
    for line in pathlib.Path(path).read_text().splitlines():
        line = line.strip()
        if line == "[[asset]]":
            if entry.get("path"):
                if "*" in entry["path"]:
                    globs.append((entry["path"], entry.get("license", "?")))
                else:
                    exact[entry["path"]] = entry.get("license", "?")
            entry = {}
            continue
        m = re.match(r'(\w+)\s*=\s*"([^"]*)"', line)
        if m:
            entry[m.group(1)] = m.group(2)
    if entry.get("path"):
        if "*" in entry["path"]:
            globs.append((entry["path"], entry.get("license", "?")))
        else:
            exact[entry["path"]] = entry.get("license", "?")
    return exact, globs


def licence_of(rel, exact, globs):
    if rel in exact:
        return exact[rel]
    import fnmatch
    for pattern, lic in globs:
        if fnmatch.fnmatch(rel, pattern):
            return lic
    return None


def entries(spe):
    out = []
    for line in run(TOOL, spe, "list").decode(errors="replace").splitlines()[2:]:
        parts = line.split()
        if len(parts) < 5 or not parts[0].isdigit():
            continue
        # id type bytes crc name...
        name = line.split(parts[3], 1)[1].strip()
        name = name.split("\t")[0].strip()
        out.append((int(parts[0]), int(parts[1]), name))
    return out


def to_png(pcx, png):
    """PCX out of abuse-tool, PNG for everyone else.

    Pillow if it is there, ImageMagick if it is not, and the raw PCX if
    neither: the export is still useful, it just needs converting later.
    """
    try:
        from PIL import Image
        Image.open(pcx).save(png)
        return "png"
    except ImportError:
        pass
    except Exception as e:
        print(f"  {pcx.name}: {e}", file=sys.stderr)
        return None

    if subprocess.run(["magick", str(pcx), str(png)],
                      capture_output=True).returncode == 0:
        return "png"
    return "pcx"


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="*",
                    help="the .spe files to export; default is all of data/art")
    ap.add_argument("--out", default="build/export")
    ap.add_argument("--data", default="data")
    args = ap.parse_args()

    data = pathlib.Path(args.data)
    files = [pathlib.Path(f) for f in args.files] or sorted(data.rglob("*.spe"))
    if not files:
        sys.exit(f"no .spe files under {data}")

    exact, globs = load_manifest(data / "MANIFEST.toml")
    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    tmp = out / ".pcx"
    tmp.mkdir(exist_ok=True)

    index, skipped, kept = [], [], 0

    for spe in files:
        rel = str(spe.relative_to(data)) if data in spe.parents else str(spe)

        # Never the original data. It is not in the repository, it is not
        # ours, and rule 1 says it is not to be touched.
        if rel.startswith("classic/") or "/classic/" in str(spe):
            skipped.append((rel, "original data, never exported"))
            continue

        lic = licence_of(rel, exact, globs)
        if lic is None:
            skipped.append((rel, "no manifest entry"))
            continue
        if lic not in MAY_MODIFY:
            skipped.append((rel, f"licence {lic} does not allow modification"))
            continue

        got = 0
        for eid, etype, name in entries(spe):
            if etype not in IMAGE_TYPES:
                continue

            # Some entries are named after the file they were imported
            # from in 1995 and still carry .pcx; keeping it would produce
            # foo.pcx.png.
            base = name[:-4] if name.lower().endswith(".pcx") else name
            safe = re.sub(r"[^A-Za-z0-9_.-]", "_", base) or f"entry{eid}"
            stem = spe.stem
            folder = out / spe.parent.name / stem
            folder.mkdir(parents=True, exist_ok=True)

            pcx = tmp / f"{stem}-{eid}.pcx"
            pcx.write_bytes(run(TOOL, str(spe), "getpcx", str(eid), check=False))
            if pcx.stat().st_size == 0:
                pcx.unlink()
                continue

            png = folder / f"{safe}.png"
            kind = to_png(pcx, png)
            if kind != "png":
                png = folder / f"{safe}.pcx"
                png.write_bytes(pcx.read_bytes())
            pcx.unlink()

            index.append({
                "source": rel,
                "entry": eid,
                "name": name,
                "type": IMAGE_TYPES[etype],
                "licence": lic,
                "file": str(png.relative_to(out)),
            })
            got += 1
            kept += 1

        if got:
            print(f"{rel}: {got} images")

    tmp.rmdir()
    (out / "index.json").write_text(json.dumps(index, indent=1, sort_keys=True))

    print(f"\n{kept} images from {len(files)} files -> {out}")
    print(f"index: {out / 'index.json'}")
    if skipped:
        print(f"\nskipped {len(skipped)}:")
        for rel, why in skipped[:10]:
            print(f"  {rel}: {why}")
        if len(skipped) > 10:
            print(f"  ... and {len(skipped) - 10} more")


if __name__ == "__main__":
    main()
