# VeloHUD for MetaHookSv

A MetaHookSv plugin to display player velocity with prediction and filtering support.

## Features
- **FPS-independent Filtering**: Velocity display remains smooth regardless of your framerate.
- **Hybrid Prediction**: Ability to combine engine velocity with client-side prediction.
- **Flexible Color Support**: Supports "R G B A" format (use quotes in console).
- **Relative Coordinates**: Positioning using screen ratios (0.0 - 1.0).

## CVars
- `cl_velocity_enabled` (1/0) — Enable/Disable HUD.
- `cl_velocity_source` (0/1/2) — Data source: 0 - Predict (Client), 1 - Engine, 2 - Hybrid.
- `cl_velocity_mode` (1/0) — 1 - Filtered (smooth), 0 - Raw velocity.
- `cl_velocity_pos_x` (0.0 - 1.0) — Horizontal position.
- `cl_velocity_pos_y` (0.0 - 1.0) — Vertical position.
- `cl_velocity_font_size` — Font size (in pixels).
- `cl_velocity_color` — Color in `"R G B A"` format. Partial input supported (e.g., `255` makes text red).
- `cl_velocity_show_max` (1/0) — Display maximum reached velocity.

## Commands
- `cl_velocity_reset_max` — Reset the maximum velocity counter.
- `cl_velocity_help` — Print help and current status to the console.
