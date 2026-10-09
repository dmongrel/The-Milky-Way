# The Milky Way — build prompt

You are building a Windows screen saver called **The Milky Way**. Your build is one entry in a
comparison: several AI models receive this same prompt, and each one's project is judged on the
quality of its code, its correctness, and how faithfully it follows this specification. Read the
whole prompt before you do anything.

Your model name for this run is **<LLM NAME>**.

## 1. Set up your project from the template

`C:\_C_Projects\The Milky Way` is the template. Before anything else, copy it to your own project
directory, a sibling of the template named with your model name:

    C:\_C_Projects\The Milky Way-<LLM NAME>

Copy everything except the `.git`, `build`, `cmake-build-*` and `.idea` directories. For example:

    robocopy "C:\_C_Projects\The Milky Way" "C:\_C_Projects\The Milky Way-<LLM NAME>" /E /XD .git build cmake-build-debug cmake-build-release .idea

(robocopy exit codes 0 to 7 mean success.)

From then on, work only inside your copy:

- Do not modify the template, and do not read anything outside your copy except to make it.
- Do not read `C:\_C_Projects\The Black Hole` or any other `The Milky Way-*` directory. Everything
  you need from outside is already in the template; the rest of the code is yours to write.

Two names derived from the model name are used below:

- **`<slug>`**: the model name lowercased, with every run of characters outside `a-z 0-9`
  replaced by a single `-`, and no leading or trailing `-`. `qwen122b` stays `qwen122b`;
  `Qwen3.5-122B` becomes `qwen3-5-122b`.
- **`<repo>`**: `The-Milky-Way-` followed by the model name with every character outside
  `A-Z a-z 0-9 . _ -` replaced by `-`.

## 2. What the template gives you

| Path                                   | What it is                                                     |
|----------------------------------------|----------------------------------------------------------------|
| `CMakeLists.txt`                       | The build: compiles `src/**/*.cpp`, volk and the resource script; compiles every shader in `shaders/` with `glslc -Werror`; embeds the SPIR-V. |
| `cmake/EmbedShaders.cmake`             | Turns the SPIR-V files into `build/gen/shaders_generated.c`.   |
| `src/render/shaders_embedded.{h,cpp}`  | `shader_blob("galaxy.frag")` returns an embedded blob, or null. |
| `shaders/fullscreen.vert`              | A full-screen triangle generated from `gl_VertexIndex`.        |
| `shaders/galaxy.frag`                  | Placeholder that outputs opaque black.                         |
| `third_party/volk/`                    | The volk Vulkan loader, unmodified.                            |
| `assets/fonts/`                        | Michroma and its SIL Open Font License, unmodified.            |
| `the-milky-way.rc`, `src/resource.h`   | The Settings-list name, the font and licence as `RCDATA`, the manifest. |
| `the-milky-way.manifest`               | Per-monitor-v2 DPI awareness, Common Controls v6.              |
| `src/main.cpp`                         | A stub `wWinMain` that exits 0. Replace it.                    |
| `LICENSE`, `THIRD_PARTY_NOTICES.md`, `.gitignore` | Keep them; update them if what you add requires it. |

Treat the template's build files as yours: change them where your design needs it, but keep every
decision in section 3.

## 3. Technical decisions (fixed, not yours to change)

| Area            | Decision                                                                          |
|-----------------|-----------------------------------------------------------------------------------|
| Language        | C++20. C only for vendored third-party code.                                      |
| Platform        | Win32 API, Unicode throughout (`wWinMain`, `-municode`, `UNICODE`/`_UNICODE`).    |
| Toolchain       | MSYS2 UCRT64 `gcc`, `g++`, `windres` and `glslc`, in `C:\msys64\ucrt64\bin`.     |
| Build system    | CMake with the Ninja generator. CMake and Ninja are CLion's: `%LOCALAPPDATA%\Programs\CLion\bin\cmake\win\x64\bin` and `%LOCALAPPDATA%\Programs\CLion\bin\ninja\win\x64`. |
| Output          | One statically linked GUI executable, `build/the-milky-way-<slug>.scr`. Set `SAVER_NAME` in `CMakeLists.txt`. |
| Graphics API    | Vulkan, loaded at run time through volk with `VK_NO_PROTOTYPES`. Nothing may import `vulkan-1.dll` statically. |
| Shaders         | GLSL, compiled at build time and embedded. Never read from disk at run time.      |
| Overlay font    | Michroma, for every piece of text drawn on screen, loaded from the embedded resource with `AddFontMemResourceEx`; never installed. |
| Warnings        | `-Wall -Wextra`. The build must produce zero warnings.                            |
| License         | MIT, `Copyright (c) 2026 dmongrel`.                                               |

## 4. Stage 1: the screen-saver shell

Set the name Windows lists in Screen Saver Settings (string 1 in `the-milky-way.rc`) to
`The Milky Way (<LLM NAME>)`.

Then write the shell: argument handling, windows, message loop, input, logging. Keep `main.cpp`
to parsing and dispatch; put the rest under `src/app/`.

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
must still run the saver. A screen saver must never show an error dialog in any mode except `/c`.

Logging: when the environment variable `MILKY_WAY_LOG` is `1`, write a timestamped trace to
`%TEMP%\the-milky-way-<slug>.log`; any other non-empty value is the log file's path. With it
unset, write nothing.

Update `README.md` to describe your build: what it is, how to build it, the argument table, the
logging variable, and the layout.

Keep code minimal and clear, with comments that explain *why* rather than *what*. Add nothing this
stage does not ask for: no renderer, no Vulkan instance yet, no settings storage.

## 5. Git and GitHub

1. `git init -b main` in your project directory.
2. Set the identity **for this repository only**:
   `git config user.name dmongrel` and `git config user.email deathmongrel@gmail.com`.
   Do not change global git configuration.
3. Commit the template exactly as you copied it, with the message
   `Start from The Milky Way template`, so your own changes show as a separate diff.
4. Commit your stage 1 work. Do not commit build outputs. The message is a short summary line plus
   a body describing what you built. Do not add `Co-Authored-By`, "Generated by", or any other
   AI-attribution line.
5. Create the remote with the GitHub CLI, which is already signed in:

       gh repo create dmongrel/<repo> --private --description "The Milky Way screen saver, built by <LLM NAME>" --source . --remote origin --push

6. Do not open pull requests. Do not create other branches.

## 6. Verify before you report

Run each check and report its actual output. Fix and re-run anything that fails.

1. With `C:\msys64\ucrt64\bin` and CLion's CMake and Ninja directories first on `PATH`,
   `cmake -S . -B build -G Ninja` and `cmake --build build` succeed from a fresh `build`
   directory with **zero** warnings, and `build/the-milky-way-<slug>.scr` exists.
2. `build/gen/shaders_generated.c` names both shaders.
3. Starting the `.scr` with the argument `/x` exits with code 0 within a few seconds.
4. `git status` is clean, and `git log` shows both commits on `main`.
5. `git remote -v` shows `origin` at `https://github.com/dmongrel/<repo>.git`, and `main`
   tracks `origin/main`.

Do **not** run the saver with no argument, `/s`, `/w` or `/c`: those take over the screen or need
a person at the keyboard.

## 7. Report

Finish with a short report:

- The directory and repository URL you created.
- Each verification check and its result.
- Every place you departed from this specification, and why.
- Anything you were unsure of.
