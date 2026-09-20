# spe-export

Pulls the art out of `.spe` files as PNG, with an index and a licence check.

```sh
./tools/spe-export/spe-export.py --out build/export
./tools/spe-export/spe-export.py data/art/fore/techno.spe --out /tmp/x
```

Over the whole tree that is **2848 images from 184 files**, all of them
public domain.

## What it is for

Phase 6.6 wants an HD pack: sprites upscaled and retouched, dropped in as
`data/hd/<id>.png` with the original as the fallback. The art has to come
out first, and it has to come out with a name that survives the round trip
and a licence attached.

`index.json` records, per image, the file it came from, the entry id, the
entry name, the kind, the licence and where it was written. That is what
lets an override be matched back to the thing it replaces.

## What it refuses

- **Anything under `classic/`.** The original data is not ours and rule 1
  of `AGENTS.md` says it is never modified. It is not in the repository
  either; this is belt and braces.
- **Anything with no manifest entry**, because then its licence is unknown.
- **Anything whose licence does not allow modification.** Nothing shipped
  today is in that state, and the check is here so that adding such an
  asset fails loudly instead of quietly ending up retouched.

## What it skips

`SPEC_PARTICLE` entries are not pictures whatever their names suggest: they
are lists of coloured points, and asking for one as PCX yields a truncated
file. Fifteen of them live in `chars/lavap.spe`.

## Dependencies

`abuse-tool` from a `dev` build does the decoding, which is where the format
knowledge already lives. PNG conversion uses Pillow if it is installed and
ImageMagick otherwise; with neither, the PCX is written as it came out and
can be converted later.
