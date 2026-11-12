# CLAUDE.md: m5-ui

UI components and design tokens for M5Unified on 320x240 displays. Usage: `README.md`. Planned work: `TODO.md`. Local-only notes: `CLAUDE.local.md` (not in git).

## Rules

- No app-specific text, thresholds or behavior. Those stay in the app.
- Only drawing and tokens. Colors, sizes and timings are tokens the app can override.
- Release with git tags (`v0.1.0`, ...). Apps pin a tag in `lib_deps` and switch to `symlink://` only while changing both. Breaking changes bump the major or minor version and are announced to users.
- Public repository. Code, comments and docs are concise English in ASCII only. Do not mention other projects, local paths or machine names in files or commit messages.
- Target: CoreS3, PlatformIO espressif32@6.9.0 (Arduino 2.0.17), M5Unified 0.2.29, M5GFX 0.2.36, C++11.
