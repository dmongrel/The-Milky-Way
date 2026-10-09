# The Milky Way — build prompt

<!--
Operator notes (not part of the prompt; strip everything above the first rule line before use):
- Replace every <LLM NAME> with the model's name, e.g. "qwen122b" gives the project directory
  C:\_C_Projects\The Milky Way-qwen122b.
- The prompt is given verbatim, in one piece, to every model under test, so results compare.
- Stage 1 (this file, so far) is the scaffold only. Later stages get appended as new sections.
-->

---

You are building a Windows screen saver called **The Milky Way**. Your build is one entry in a
comparison: several AI models receive this same prompt, and each one's project is judged on the
quality of its code, its correctness, and how faithfully it follows this specification. Read the
whole prompt before you do anything.

Your model name for this run is **<LLM NAME>**.

## 1. Set up your project

Before anything else, create your own project directory as a sibling of the reference project:

    C:\_C_Projects\The Milky Way-<LLM NAME>

If the model name contains a character Windows forbids in a path (`\ / : * ? " < > |`), replace
each such character with `-`.

Everything you create goes inside that directory. Do not modify any file outside it.

Two names derived from the model name are used below:

- **`<slug>`**: the model name lowercased, with every run of characters outside `a-z 0-9`
  replaced by a single `-`, and no leading or trailing `-`. "qwen122b" stays `qwen122b`;
  "Qwen3.5-122B" becomes `qwen3-5-122b`.
- **`<repo>`**: `The-Milky-Way-` followed by the model name with every character outside
  `A-Z a-z 0-9 . _ -` replaced by `-`.

### What you may read

- From `C:\_C_Projects\The Black Hole`, only the third-party files listed in section 4: the
  Michroma font with its licence, and volk. Copy them; do not open anything else in that
  directory. Every other line of the project is yours to write.
- Do **not** read `C:\_C_Projects\The Milky Way` or any other `The Milky Way-*` directory. Those
  are other entries in the comparison.

## 2. Technical decisions (fixed, not yours to change)

| Area            | Decision                                                                          |
|-----------------|-----------------------------------------------------------------------------------|
| Language        | C++20. C only for vendored third-party code.                                      |
| Platform        | Win32 API, Unicode throughout (`wWinMain`, `-municode`, `UNICODE`/`_UNICODE`).    |
| Toolchain       | MSYS2 UCRT64: `g++`, `gcc`, `windres` in `C:\msys64\ucrt64\bin`; GNU `make` in `C:\msys64\usr\bin`. |
| Build system    | One hand-written GNU `Makefile`. No CMake, no IDE project files.                  |
| Output          | A single statically linked GUI executable named `the-milky-way-<slug>.scr` (`-mwindows -static`). |
| Graphics API    | Vulkan, loaded at run time through **volk** with `VK_NO_PROTOTYPES`. Nothing may import `vulkan-1.dll` statically. |
| Shaders         | GLSL compiled by `glslc -Werror` to SPIR-V at build time and embedded in the executable. Never read from disk at run time. |
| Overlay font    | **Michroma**, for every piece of text drawn on screen. Embedded in the `.scr` as an `RCDATA` resource together with its OFL licence. Loaded with `AddFontMemResourceEx`; never installed. |
| Warnings        | `-Wall -Wextra`. The build must produce zero warnings.                            |
| License         | MIT, `Copyright (c) 2026 dmongrel`.                                               |

## 3. Screen-saver behaviour the scaffold must already have

Windows starts a screen saver with one of these arguments. Accept `/` or `-` as the prefix and
treat the letter case-insensitively.

| Argument                    | Behaviour                                                                 |
|-----------------------------|---------------------------------------------------------------------------|
| none, `/s`                  | Full screen: one borderless top-most window per monitor, cursor hidden.   |
| `/p <hwnd>`, `/p:<hwnd>`    | Preview: a child window filling the given parent's client area. Exit when the parent window disappears. Exit 0 if the handle is missing or invalid. |
| `/c`, `/c:<hwnd>`           | Settings: a message box, owned by the window if one is given, saying there are no settings yet. |
| `/w`                        | Development: an ordinary resizable 1600×900 window. Esc or closing it exits. |
| anything else               | Exit immediately with code 0, showing nothing.                            |

Rules for ending a full-screen run:

- Any key press, any mouse button, or mouse movement of more than 8 pixels ends the run.
- Ignore all input for the first 0.5 seconds; the key that launched the saver is often still held.
- A key already held when that grace period ends does not count until it has been released.
- Input must end the run even when no window of yours has focus, so poll keyboard and cursor
  state each frame as well as handling window messages.
- A failure to read input (for example `GetCursorPos` failing on another desktop) is not input.

At this stage nothing is rendered: every window paints black. A machine without a Vulkan driver
must still run the saver (black, never a crash or an error dialog). A screen saver must never
show an error dialog in any mode except `/c`.

Logging: when the environment variable `MILKY_WAY_LOG` is `1`, append a timestamped trace to
`%TEMP%\the-milky-way-<slug>.log`; any other non-empty value is the log file's path. With it unset,
write nothing.

## 4. Scaffold contents

Create this layout:

```
Makefile
README.md
LICENSE
THIRD_PARTY_NOTICES.md
.gitignore
the-milky-way-<slug>.rc
the-milky-way-<slug>.manifest
src/main.cpp                 argument parsing and dispatch only
src/app/                     the host: arguments, windows, message loop, input, logging
src/render/                  embedded-shader lookup; the renderer will live here
shaders/fullscreen.vert      a full-screen triangle generated from gl_VertexIndex
shaders/galaxy.frag          placeholder that outputs opaque black
assets/fonts/                Michroma-Regular.ttf and Michroma-OFL.txt
third_party/volk/            volk.c, volk.h, LICENSE.md
tools/                       the script that turns SPIR-V files into a C source file
docs/                        empty for now
```

Copy these files unmodified from The Black Hole:

- `assets/fonts/Michroma-Regular.ttf` and `assets/fonts/Michroma-OFL.txt`
- `third_party/volk/volk.c`, `volk.h` and `LICENSE.md`

Requirements for the individual files:

- **Resource script.** String table entry 1 is the name Windows lists in Screen Saver Settings:
  `The Milky Way (<LLM NAME>)`. Embed the font and its licence as `RCDATA`, with their ids in a
  header shared with the code. Embed the manifest.
- **Manifest.** Per-monitor-v2 DPI awareness, Windows 10/11 compatibility, Common Controls v6.
- **Makefile.** Targets `all` (default), `clean`, and `install`. Compile each shader to
  `build/spv/`, generate one C file embedding all SPIR-V blobs into `build/gen/`, put objects
  under `build/obj/`, and track header dependencies (`-MMD -MP`) so editing a header rebuilds
  what includes it. Keep compiler temp files inside `build/`. `install` copies the `.scr` to
  `%LOCALAPPDATA%\The-Milky-Way-<slug>\` and selects it as the current user's screen saver.
- **.gitignore.** Build outputs (`build/`, `*.scr`, `*.exe`, `*.o`, `*.res`), IDE folders, and
  Windows clutter.
- **README.md.** What the project is, how to build it, the argument table, the logging variable,
  and the layout.
- **THIRD_PARTY_NOTICES.md.** Michroma (copyright, OFL 1.1, why the licence is embedded, the
  font file's SHA-256 and size) and volk (copyright, MIT).

Keep code minimal and clear, with comments that explain *why* rather than *what*. Add nothing this
section does not ask for: no renderer, no Vulkan instance yet, no settings storage.

## 5. Git and GitHub

1. `git init -b main` in your project directory.
2. Set the identity **for this repository only**:
   `git config user.name dmongrel` and `git config user.email deathmongrel@gmail.com`.
   Do not change global git configuration.
3. Commit the scaffold. Do not commit build outputs. The commit message is a short summary line
   plus a body describing what the scaffold contains. Do not add `Co-Authored-By`, "Generated by",
   or any other AI-attribution line.
4. Create the remote with the GitHub CLI, which is already signed in:

       gh repo create dmongrel/<repo> --private --description "The Milky Way screen saver, built by <LLM NAME>" --source . --remote origin --push

5. Do not open pull requests. Do not create other branches.

## 6. Verify before you report

Run each check and report its actual output. Fix and re-run anything that fails.

1. `make clean && make` with `C:\msys64\ucrt64\bin` and `C:\msys64\usr\bin` first on `PATH`
   succeeds with **zero** warnings, and `the-milky-way-<slug>.scr` exists.
2. Both shaders compiled, and the generated embedding file names both blobs.
3. Starting the `.scr` with the argument `/x` exits with code 0 within a few seconds.
4. `git status` is clean and `git log` shows your commit on `main`.
5. `git remote -v` shows `origin` at `https://github.com/dmongrel/<repo>.git`, and `main`
   tracks `origin/main`.

Do **not** run the saver with no argument, `/s`, `/w` or `/c`, and do **not** run `make install`:
those take over the screen, need a person at the keyboard, or change system settings.

## 7. Report

Finish with a short report:

- The directory and repository URL you created.
- Each verification check and its result.
- Every place you departed from this specification, and why.
- Anything you were unsure of.
