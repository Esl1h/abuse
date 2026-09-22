# Packaging

Skeletons for the four channels of phase 8. **None of these has been built
yet**: they carry the identity (name, App ID, licence, description) so that
everything downstream agrees on it, and the build recipes still need to be
finished and run.

| Channel | Directory | Package name |
| --- | --- | --- |
| Flathub | `flatpak/` | `io.github.Esl1h.AbuseVrenna` |
| AUR | `aur/` | `abuse-vrenna`, `abuse-vrenna-git` |
| AppImage | `appimage/` | `Abuse_Vrenna-x86_64.AppImage` |
| Windows, macOS | (CI) | zip and dmg from the release workflow |

## Identity

- **Title**: Abuse: Vrenna
- **Summary**: Modern port of Abuse (1995)
- **App ID**: `io.github.Esl1h.AbuseVrenna`
- **Binary**: `abuse-vrenna`
- **Licence**: GPL-2.0-or-later for the code; see `data/MANIFEST.toml` for data

The repository is `Esl1h/abuse-vrenna`, renamed from `Esl1h/abuse` on
2026-09-22 to match. GitHub redirects the old URL, so a clone made before the
rename keeps working.

The App ID keeps its capitals, `io.github.Esl1h.AbuseVrenna`, and does not
have to follow the repository name. AppStream accepts it: `appstreamcli
validate` passes, with one pedantic note that a component ID should be all
lowercase. A hyphen in an ID is fine, measured the same way, so
`io.github.esl1h.abuse-vrenna` would validate with nothing at all. Changing
it renames four files and the manifest, and is a decision of its own.

The App ID and the package names carry `vrenna` because `abuse` is already
taken on both Flathub and the AUR: `com.github.Xenoveritas.abuse` is the
upstream of this fork, and `abuse` and `abuse-git` are the SDL 1.2 port.

**The user directory is not renamed.** Config and saves stay under
`$XDG_CONFIG_HOME/abuse/` and `$XDG_DATA_HOME/abuse/`, where they have always
been. That path is not the product name, and moving it would orphan every
existing save for a cosmetic reason.

## Original data

The original sound and music are not redistributable and are in no package
here. Both the Flatpak and the native packages ship
`scripts/fetch-classic-data.sh`, which the player runs once; the Original mode
finds the result on its own.
