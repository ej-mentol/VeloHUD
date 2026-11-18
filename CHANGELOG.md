# Changelog

All notable changes to VeloHUD are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.3.0] - 2026-11-18

### Added

- Standalone CMake build with `build_plugin.bat` / `build_plugin.ps1`, the
  shared formatting check, and an optional `LaunchGame` Visual Studio target.
- Opt-in `HUD_PlayerMove` diagnostics (`-DVELOHUD_DIAGNOSTICS=ON`, or
  `build_plugin.bat -Diagnostics`): average and peak calls per redraw, the
  number of frames without a move, and the integrated `frametime` versus the
  real redraw delta.

### Changed

- `velo_mode` replaces the old `cl_velocity_enabled`, `cl_velocity_source` and
  `cl_velocity_mode` cvars; `0` disables the HUD and the value selects the
  source.
- `velo_hide_max` and `velo_hide_vertical` replace the old `velo_show_*` names.
- Vertical speed is sampled only while the second line is visible.
- `velo_reset_max` also restarts the diagnostic measurement window.

### Fixed

- `Con_Printf` is called with a `"%s"` format, so a crafted `velo_color` value
  can no longer be interpreted as a format string.
- Non-finite velocities and frametimes no longer poison the filter or drop the
  displayed value to zero for a frame.
- Cvar values are clamped where they are read instead of being rewritten every
  frame; the archived values keep exactly what the user typed.
- The cached VGUI2 font handle survives map changes and is dropped only when
  VGUI2Extension unloads; a missing handle is recreated lazily.
- The vertical speed buffer has room for the worst-case formatted values.

### Removed

- `VeloHUD.vcxproj` and `VeloHUD.vcxproj.filters`; CMake generates the Visual
  Studio project files.
