# TODO

State as of 2026-09-18. The detailed planning lives in `docs/`, which is local and stays
out of the repository; this file is the public summary.

## Done in this fork

- **Foundation**: CMake presets (`dev`, `release`, `asan`, `headless`), a test harness
  with deterministic replay, a state hash and frame snapshots
- **Data and licensing**: `data/MANIFEST.toml` with origin and licence per file,
  `scripts/check-licenses.sh` in CI, XDG paths per mode, a script that downloads the
  original data
- **SDL3**: port finished, scale modes, filter, vsync, FPS limit
- **Fixed timestep**: 15 Hz of logic decoupled from the frame
- **Gamepad**: action map with several bindings per action, aiming on the right stick,
  optional aim assist, rumble, button glyphs per controller family, hot-plug that
  pauses the game
- **New UI**: a native resolution layer over the already scaled frame, with language,
  options and controls screens, all reachable by keyboard and by controller
- **i18n**: language selection with locale detection, a pt-BR translation, a pseudo
  language for finding text that does not fit, and a font with full Latin coverage

## Open

### Phase 4, what is left

- [ ] Human review of the pt-BR translation
- [ ] New HUD: health with damage animation, weapon and ammunition, power-ups with a
      timer, directional damage indicator, a "classic HUD" preset
- [ ] New start menu: Play (Original / Remastered), Continue, Options, Credits
- [ ] Visual theme for the new screens: their own palette and font. They are
      deliberately plain today
- [ ] Remaining widgets: slider, checkbox toggle, modal dialog
- [ ] Audio options, once there is audio

### Phase 5, audio

- [ ] Mixer with separate buses and independent volumes
- [ ] A free sound set for the Remastered mode. The Golgotha pack, public domain,
      covers 19 of the 78 events; the other 59 need a source
- [ ] Free music

### Later phases

- [ ] 6: optional lighting and shaders, with the classic preset untouched
- [ ] 7: widescreen without changing the logic
- [ ] 8: optional HD pack, Flatpak, AUR, AppImage, release
- [ ] 9: new content, after 1.0

### Known debt

- [ ] Closing phase 0: CI on a runner, hand-played replays, branch protection
- [ ] The three replays in `tests/replays/` are synthetic: 400 ticks with no input.
      They exercise neither combat nor dynamic light
- [ ] `SDL_native_midi` forces debug logging; report it upstream
- [ ] `vcpkg.json` is a leftover of the SDL2 path and takes no part in the build

## Inherited from upstream

### Xenoveritas

- [x] Add joystick/gamepad support
- [ ] Find any dead code and remove it
- [x] Rewrite event handling to allow multiple keybindings for the same actions
- [x] Update configuration system to deal with joysticks/gamepads
- [x] Change default control bindings to work on modern PCs:
   - [x] For trackpad users, add another key to activate special abilities
   - [x] Enable WADS movement instead of arrow keys
   - [x] Add next/previous weapon keys closer to WADS
- [ ] Strongly considering replacing the jFILE/bFILE stuff with SDL's SDL_RW API

### Original

- [ ] Go through the old stuff below and figure out what's even still relevant:

----

This is a list of known bugs and features that need fixing/implementing:

FEATURES
--------
- Multiplayer support over the internet and local network using tcp/ip.
- Multiplayer support over a local network using IPX.
- Performance improvements.
- Add YUV overlay support.
- Convert all internal rendering to 24-bit.

SAM'S TODO
----------
 - replace `write_PCX` calls with `SDL_WriteSurfaceBMP`

ABUSE-TOOL
----------
 - allow to query ids by name rather than by number

DATA MERGE
----------
 - remove registration related code
   - server check in src/net/netdrv.cp
   - server check in src/innet.cpp
   - Lisp symbols server_not_reg and net_not_reg
 - ensure gamma.lsp, hardness.lsp, defaults.prp, edit.lsp etc. are always
   loaded and saved in the config directory, not in the datadir (use
   local_load instead of load?). Same for addon/deathmat/cur_lev.lsp
