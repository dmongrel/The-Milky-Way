# The Milky Way

A Windows screen saver of the Milky Way, rendered with Vulkan. On-screen overlays are set in
Michroma, the typeface The Black Hole uses.

Status: scaffold. The screen-saver shell works (full screen on every monitor, the Settings
preview pane, a development window, dismissal on input); the windows are black until the
renderer lands. The build already compiles the shaders to SPIR-V and embeds them, links volk,
and embeds the font and its licence.

## Building

MSYS2 UCRT64 with `mingw-w64-ucrt-x86_64-gcc`, `mingw-w64-ucrt-x86_64-shaderc` (for `glslc`),
`xxd` and `make`:

```sh
make            # builds the-milky-way.scr
make install    # copies it to %LOCALAPPDATA%\The-Milky-Way and selects it as the screen saver
make clean
```

## Running

| Argument      | Mode                                                        |
|---------------|-------------------------------------------------------------|
| *(none)*, `/s`| Full screen on every monitor; any input ends it             |
| `/p <hwnd>`   | Live preview in the Screen Saver Settings pane              |
| `/c`          | Settings (none yet)                                         |
| `/w`          | Resizable development window; Esc closes it                 |

`MILKY_WAY_LOG=1` writes a trace to `%TEMP%\the-milky-way.log`; any other value is a path.

## Layout

```
src/main.cpp        argument dispatch
src/app/            screen-saver host: args, windows, input, log
src/render/         renderer; for now only the embedded-shader lookup
shaders/            GLSL, compiled by glslc and embedded in the .scr
assets/fonts/       Michroma and its OFL licence, embedded in the .scr
third_party/volk/   Vulkan loader
docs/               design notes
tools/              build helpers
```

The Black Hole (`C:\_C_Projects\The Black Hole`) is the technical reference for the Vulkan
renderer and the text overlay.

## License

MIT. See [LICENSE](LICENSE). Third-party work: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
