# VeloHUD

Velocity HUD for MetaHookSv (GoldSrc / Sven Co-op). It hooks `HUD_PlayerMove`
and `HUD_Redraw`, reads the client-predicted and engine velocities, and draws a
single text line through `ISurface2`.

## Features

- **Frame-rate independent filtering**: an exponential filter driven by
  `frametime`, so the smoothing does not change with FPS.
- **Four data sources**: raw prediction, filtered prediction, engine state, and
  two hybrid mixes.
- **Optional vertical speed** on a second line, sampled only when enabled.
- **Relative positioning**: `velo_pos_x` / `velo_pos_y` are screen ratios.
- Text color is `"R G B A"`; missing channels fall back to `180 180 180 255`.

## CVars

| CVar | Default | Description |
| --- | --- | --- |
| `velo_mode` | `2` | `0` off; `1` prediction raw, `2` prediction filtered, `3` engine, `4` hybrid raw, `5` hybrid filtered |
| `velo_marker` | `0` | Fixed benchmark drawn as `(N)` instead of the map maximum |
| `velo_pos_x` | `0.5` | Horizontal position, `0.0 .. 1.0` of screen width |
| `velo_pos_y` | `0.85` | Vertical position, `0.0 .. 1.0` of screen height |
| `velo_font_size` | `24` | Font height in pixels, clamped to `6..512` |
| `velo_color` | `180 180 180 255` | Text color, `"R G B A"` |
| `velo_hide_max` | `0` | `1` hides the `(Max: N)` suffix, or `(marker)` when `velo_marker` is set |
| `velo_hide_vertical` | `1` | `0` shows a second line with vertical speed and its max/min for the map |

All variables are archived in `config.cfg`.

GoldSrc cvars have no min/max, so the plugin applies these rules every time
it reads a value instead of rewriting the cvar: `velo_hide_max` /
`velo_hide_vertical` treat any non-zero value as on, `velo_mode` is clamped to
`0..5`, a non-positive `velo_marker` disables the marker, `velo_pos_x` /
`velo_pos_y` are clamped to `0..1`, and `velo_font_size` is clamped to
`6..512`. The archived values stay exactly as typed, and the plugin never
writes cvars while it runs. `velo_help` prints the effective values.

The flags are named after what they turn off: `velo_hide_max` and
`velo_hide_vertical` are `0` (shown) by default, so the settings read as "hide
X" rather than "show X". The old `velo_show_*` names are not registered any
more; if a `config.cfg` still contains them, delete those lines or ignore them
— they have no effect.

`velo_mode` replaces the old `cl_velocity_enabled` + `cl_velocity_source` +
`cl_velocity_mode` trio: `0` turns the HUD off and the value itself selects the
source, so there is no meaningless "filtered engine" combination. Existing
`cl_velocity_*` settings are not read.

## Commands

- `velo_reset_max` — reset the maximum speed counter.
- `velo_help` — print the current values and the available options.

## Diagnostics

Configure with `-DVELOHUD_DIAGNOSTICS=ON` (or build with
`build_plugin.bat -Diagnostics`) to compile in two `velo_help` lines: average/peak `HUD_PlayerMove` calls per redraw with the number of
zero-move frames, and the average `Σ frametime` per frame against the real
redraw delta. It is off by default and has no runtime cost when disabled. The
counters restart on a map change and when `velo_reset_max` is executed, so a
segment can be measured without changing the map.

The decisive number is the dt ratio: `1.00` means the filter integrated
exactly the elapsed frame time, no matter how many calls the engine made.
Above `1.00` the filter over-smooths, below `1.00` it advances too slowly
(typically the zero-move frames), and both argue for moving the filter into
`HUD_Redraw`.

A local test exercises the same math: `fps_max 15` together with
`cl_cmdrate 66` can produce several calls per frame. Ping only changes how
many commands are replayed; it does not change what the ratio means.

## Build

Windows x86, MSVC (Visual Studio 2022), static CRT, CMake 3.21+.

Fast path for a standalone copy:

```bat
build_plugin.bat
build_plugin.bat -Diagnostics
```

The script finds MetaHook and VGUI2Extension next to the plugin (or through
`METAHOOK_SOURCE_PATH` / `VGUI2EXTENSION_SOURCE_PATH`), configures into `build/`
and reports the produced DLL. Pass `-Install` to also stage
`svencoop/metahook/plugins/` into `install/`. `-Diagnostics` enables the
opt-in `HUD_PlayerMove` call-rate counters (off by default).

Manual CMake:

```bat
cmake -S . -B build -A Win32 ^
  -DMETAHOOK_SOURCE_PATH=..\..\MetaHook ^
  -DVGUI2EXTENSION_SOURCE_PATH=..\VGUI2Extension
cmake --build build --config Release
cmake --install build --config Release --prefix install
```

Empty `METAHOOK_SOURCE_PATH` fetches the MetaHook SDK from GitHub; empty
`VGUI2EXTENSION_SOURCE_PATH` fetches the pinned VGUI2Extension commit.

## Notes

- The HUD needs `VGUI2Extension.dll` at runtime; it is loaded dynamically and
  re-acquired if the module is reloaded.
- `velo_font_size` creates a new font handle per distinct size because VGUI2's
  `AddGlyphSetToFont` refuses a font that already owns glyph sets.

## Changelog

See [CHANGELOG.md](CHANGELOG.md).

## License

MIT — see `LICENSE`. No third-party code is bundled.
