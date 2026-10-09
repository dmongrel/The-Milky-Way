# The Milky Way — build prompt

You are building a Windows screen saver called **The Milky Way**: a simulation of our galaxy, seen
from a camera orbiting it. Your build is one entry in a comparison: several AI models receive this
same prompt, and each one's project is judged on the quality of its code, its correctness, how
good the result looks, and how faithfully it follows this specification. Read the whole prompt
before you do anything.

Your model name for this run is **<LLM NAME>**.

## 0. How to work

- **Work autonomously.** Nobody will answer questions during the run. When something is unclear,
  decide it yourself with your best judgement, note the decision, and carry on. List those
  decisions in your final report.
- **Physics and astronomy questions go to the web, not to the user.** The user is not a physicist
  and cannot answer them. Search, read the sources, and save what you learn (section 1).
- **Aesthetics come first, but do not throw reality away.** Where accuracy and beauty conflict,
  favour beauty, keep the result plausible, and say in your report what you changed and why.
- **Tests: use them sparingly.** This is a screen saver, not a production application. Write a
  test only when it proves something you need to be true, such as the input rules that let a
  person dismiss the saver.
- **Benchmarks and profiling: use them when there is a real choice.** When two ways of doing
  something could each work, measure both on this machine and keep the faster one. Report what
  you measured.
- **Look at what you make.** Give the saver a way to write a frame to an image file and exit on
  its own (section 5), and use it to check your work as you go.

## 1. Set up your project from the template

`C:\_C_Projects\The Milky Way` is the template. Before anything else, copy it to your own project
directory, a sibling of the template named with your model name:

    C:\_C_Projects\The Milky Way-<LLM NAME>

Copy everything except the `.git`, `build`, `cmake-build-*` and `.idea` directories. For example:

    robocopy "C:\_C_Projects\The Milky Way" "C:\_C_Projects\The Milky Way-<LLM NAME>" /E /XD .git build cmake-build-debug cmake-build-release .idea

(robocopy exit codes 0 to 7 mean success.)

From then on, work inside your copy, with one exception, the shared references directory:

    C:\_C_Projects\The Milky Way References

Every model in the comparison shares it. **Read it before you search the web**: another model may
already have saved what you need. Everything you gather from the web goes there, not only into
your project:

- Notes as Markdown (`.md`) or plain text, with the source URL and the date for every fact.
- Images and data files as downloaded, with their source URL, credit line and licence recorded
  next to them.
- One line per file in `INDEX.md`: the file, what it is, where it came from.
- Add to the directory; never delete or rewrite another model's files. If you disagree with one,
  add your own note saying why.

Copy what your build needs from the references into your project's `assets/`, and credit every
third-party image in `THIRD_PARTY_NOTICES.md`.

Otherwise:

- Do not modify the template.
- Do not read `C:\_C_Projects\The Black Hole` or any other `The Milky Way-*` directory. Everything
  you need from outside is in the template, the references directory and the web; the code is
  yours to write.

Use this derived name below:

- **`<slug>`**: the model name lowercased, with every run of characters outside `a-z 0-9`
  replaced by a single `-`, and no leading or trailing `-`. `qwen122b` stays `qwen122b`;
  `Qwen3.5-122B` becomes `qwen3-5-122b`.

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
| `LICENSE`, `THIRD_PARTY_NOTICES.md`, `.gitignore` | Keep them; update them as what you add requires.    |

Treat the template's files as yours: change, add or replace them as your design needs, but keep
every decision in section 3.

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
| Assets          | Embedded in the `.scr` as resources. The saver reads no files at run time.        |
| Overlay font    | Michroma, for any text drawn on screen, loaded from the embedded resource with `AddFontMemResourceEx`; never installed. |
| Warnings        | `-Wall -Wextra`. The build must produce zero warnings.                            |
| License         | MIT, `Copyright (c) 2026 dmongrel`.                                               |

The machine is an AMD Strix Halo with an integrated Radeon GPU. The saver should run smoothly at
the display's refresh rate there.

## 4. What to build

### The galaxy

A simulation of the Milky Way galaxy, as a three-dimensional model the camera moves around.

- **Rotation.** The stars rotate around the galactic centre, and the spiral arms rotate too. Both
  must turn in the correct direction relative to the arms' winding; find out from the web which
  way that is, and save your sources.
- **Structure.** Find out what the Milky Way's structure is (its centre, bar, arms, disk, and
  whatever else your sources describe) and model what makes it recognisable.
- **Timescale.** Choose a timescale at which motion is visible in both the galactic centre and
  the arms while the saver runs. Real rates will differ across the galaxy; find out how, and
  keep that character even though the clock is sped up.
- **No satellite galaxies.** Do not simulate the Magellanic Clouds or any other satellite.

### The camera

The camera orbits the galaxy model. Choose its path, distance and speed for the best view, and
keep it moving.

### The background sky

Behind the galaxy, a realistic sky of distant stars and galaxies based on NASA imagery, including
the Andromeda Galaxy. Find suitable images, save them to the references directory with their
credit and licence, and embed what you use. Remember the camera is outside the Milky Way: the sky
behind it should not contain the Milky Way's own band.

### Overlays (a second stage)

Build the overlays only after the galaxy, the camera and the sky are finished, verified with
captures, and committed. All overlay text is set in Michroma, from the embedded font (section 3).
All lines (underlines, leader lines and brackets) are drawn as lines, never as text characters.
Everything fades in and out; nothing pops.

**The title.**
- Show "The Milky Way Galaxy" in large Michroma, underlined, right-aligned in the lower-right
  corner of the screen.
- It is fixed to the screen like a film credit, and does not move with the camera.
- It appears every 3 to 4 minutes, at a randomly chosen interval within that range.
- Each time, it fades in, stays fully on screen for 10 seconds, then fades out.

**The labels.**
- There are two labels, in smaller Michroma:
  - "Andromeda", on the Andromeda Galaxy in the background sky.
  - "Sol", on the approximate position of our Sun in the galaxy model. The Sun orbits with the
    disk, so this position moves with the galaxy's rotation.
- Each label belongs to a target in 3D space. Its text and lines are placed relative to that
  target and move with it as the camera orbits and the galaxy turns, so it reads as attached to
  the target, not to the screen.
- Each label appears at random times, fades in, stays for 10 seconds, and fades out.
- The Andromeda label appears only while Andromeda is on screen. If Andromeda leaves the screen,
  its label fades out early.
- Each target is framed by a pair of square brackets, `[` before it and `]` after it, drawn as
  lines.
- Each label's text is underlined. From the end of the underline nearer the brackets, a leader
  line runs to the nearest corner of the nearer bracket.

Choose the sizes, colours, line weights, fade times and label placement for the best look, and
keep the overlays understated: the galaxy is the subject. Give `/w` a way to force the title and
both labels on at a given moment, so captures can show them.

## 5. Screen-saver behaviour

Set the name Windows lists in Screen Saver Settings (string 1 in `the-milky-way.rc`) to
`The Milky Way (<LLM NAME>)`.

Keep `main.cpp` to argument parsing and dispatch. Windows starts a screen saver with one of these
arguments. Accept `/` or `-` as the prefix and treat the letter case-insensitively.

| Argument                    | Behaviour                                                                 |
|-----------------------------|---------------------------------------------------------------------------|
| none, `/s`                  | Full screen: one borderless top-most window per monitor, cursor hidden.   |
| `/p <hwnd>`, `/p:<hwnd>`    | Preview: the saver, live, in a child window filling the given parent's client area. Exit when the parent window disappears. Exit 0 if the handle is missing or invalid. |
| `/c`, `/c:<hwnd>`           | Settings: a message box, owned by the window if one is given, saying there are no settings. |
| `/w`                        | Development: an ordinary resizable 1600×900 window. Esc or closing it exits. |
| anything else               | Exit immediately with code 0, showing nothing.                            |

Rules for ending a full-screen run:

- Any key press, any mouse button, or mouse movement of more than 8 pixels ends the run.
- Ignore all input for the first 0.5 seconds; the key that launched the saver is often still held.
- A key already held when that grace period ends does not count until it has been released.
- Input must end the run even when no window of yours has focus, so poll keyboard and cursor
  state each frame as well as handling window messages.
- A failure to read input (for example `GetCursorPos` failing on another desktop) is not input.

A machine without a Vulkan driver must still run the saver, showing black. A screen saver must
never show an error dialog in any mode except `/c`.

Logging: when the environment variable `MILKY_WAY_LOG` is `1`, write a timestamped trace to
`%TEMP%\the-milky-way-<slug>.log`; any other non-empty value is the log file's path. With it
unset, write nothing.

Frame capture: when `MILKY_WAY_CAPTURE` names an image file, the `/w` window writes the frame shown
at `MILKY_WAY_CAPTURE_AT` seconds (default 5) to that file and exits. With it unset, nothing
changes.

## 6. Git

Git is local only. Do not create a GitHub repository, add a remote, or push; that happens at the
end of the project, if the entry earns it.

1. `git init -b main` in your project directory.
2. Set the identity **for this repository only**:
   `git config user.name dmongrel` and `git config user.email deathmongrel@gmail.com`.
   Do not change global git configuration.
3. Commit the template exactly as you copied it, with the message
   `Start from The Milky Way template`, so your own changes show as a separate diff.
4. Commit as you go, one commit per coherent step, on `main`. Do not commit build outputs. Each
   message is a short summary line plus a body saying what changed and why. Do not add
   `Co-Authored-By`, "Generated by", or any other AI-attribution line.

## 7. Verify before you report

Run each check and report its actual output. Fix and re-run anything that fails.

1. With `C:\msys64\ucrt64\bin` and CLion's CMake and Ninja directories first on `PATH`,
   `cmake -S . -B build -G Ninja` and `cmake --build build` succeed from a fresh `build`
   directory with **zero** warnings, and `build/the-milky-way-<slug>.scr` exists.
2. Starting the `.scr` with the argument `/x` exits with code 0 within a few seconds.
3. Frame captures from `/w` at several times show the galaxy, its rotation between them, and the
   background sky with Andromeda. Keep the captures in `docs/captures/` and commit them.
4. Captures of the overlays: the title in the lower right, and the Andromeda and Sol labels with
   their brackets and leader lines. Include two captures some seconds apart, showing that a label
   stays attached to its moving target.
5. The frame rate in `/w` at 1600×900, from the log.
6. `git status` is clean, and `git log` shows your commits on `main`.

Do **not** run the saver with no argument, `/s` or `/c`, and do not run `/w` without
`MILKY_WAY_CAPTURE` set: those take over the screen or need a person at the keyboard.

## 8. Report

Finish with a short report:

- The directory you created.
- Each verification check and its result.
- How the overlays are drawn: the text, the lines, and the 3D anchoring of the labels.
- The rotation direction you implemented and the sources that settle it.
- Your timescale, and how fast the centre and the arms turn on screen.
- The benchmarks you ran and what they decided.
- What you saved to the references directory.
- Every decision you made on your own, every place you departed from this specification, and
  where you chose beauty over accuracy, with the reason for each.
