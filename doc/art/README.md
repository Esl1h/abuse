# Art

Promotional art. None of it is used by the game: the title screen the engine
draws is 320x200 and goes through the 256-colour palette, and these are
full-colour portraits. See `src/imlib/hd.h` for what the override pack does
accept.

| File | Size | What it is |
| --- | --- | --- |
| `key-art-wordmark.png` | 862x811 | The wordmark over the character, the one that reads smallest. Used at the top of the README |
| `key-art-tall.png` | 631x1022 | Portrait poster, wordmark above the character. The source of the picture the game shows when a level is finished |
| `key-art-foundry.png` | 738x794 | The character in a foundry |
| `social-preview.png` | 1280x640 | The card GitHub shows when the repository link is shared. Derived from `key-art-wordmark.png` by `scripts/make-social.sh` |

The application icon is not here: it is `data/freedesktop/icon-source-1024.png`,
with the sizes the packages install derived from it by `scripts/make-icons.sh`.

Origin and licence of every file: `data/MANIFEST.toml`.

## The social preview is not automatic

GitHub has no API for it. Upload `social-preview.png` by hand, in Settings,
General, Social preview.

## The trademark symbol

Gone from everything that ships. The game is public domain and this project
holds no mark, so the claim was not one it could make.

`key-art-wordmark.png` and `social-preview.png` never had one. The icon's was
erased, which was easy: it sat on transparency. `key-art-tall.png` had one on
painted background, and it was patched out by copying the stretch of gradient
40 pixels below it, which is the same smooth brown and leaves no seam.

`key-art-foundry.png` has not been checked, because nothing derives from it
yet.
