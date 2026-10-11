# The Milky Way — prompt addendum

These rules apply on top of `prompt.md`, to every entry. Each one comes from a mistake an earlier
run made and had to be told about mid-run. Where this file and `prompt.md` disagree, this file
wins.

## Running the saver

- Never start the `.scr` with `Start-Process`, `ShellExecute`, `start` or a double-click. Windows
  appends `/S` and the saver takes over the screen. Run the executable directly, only with `/x`,
  or with `/w` and `MILKY_WAY_CAPTURE` set, as section 7 allows.

## Building

- The only build that counts is the one in section 7: `C:\msys64\ucrt64\bin` and CLion's CMake
  and Ninja directories first on `PATH`, then `cmake -S . -B build -G Ninja` once and
  `cmake --build build` after every change. Compiling one file with `g++ -c` is not a build: it
  skips the other sources, the link and the resources.
- Read the build output every time. Empty output is not proof of success when the command was
  piped through `head` or `tail`; check the exit code and that the `.scr` exists.
- Use the real Vulkan, Win32 and C++ names. When the compiler says a name does not exist, look it
  up in the header (`third_party/volk/`, the Vulkan headers, `windows.h`) instead of guessing
  another spelling.

## Committing

- Commit only a working step: the build finishes with no errors and
  `build/the-milky-way-<slug>.scr` exists. Never commit code you know does not compile.
- Commit after each working step, so a broken attempt costs one step, not the run.
- A commit message describes what the commit does, not what you intend. Code that exists but is
  not yet called from `main.cpp` is "added, not yet wired in", not a working feature.

## Claims

- Claim nothing you have not verified. A commit message or the report says a thing works only
  after you have built it, run it, and looked at a capture that shows it.

## Files and edits

- Keep each source file under about 300 lines. When a file grows past that, split it by moving
  whole functions into new files, with shared declarations in a small internal header.
- Change existing files with targeted edits. Do not rewrite a whole file to fix a few lines.

## Third-party code and data

- Everything you add from a third party (a library, a header, an image, a data file) gets a
  section in `THIRD_PARTY_NOTICES.md` in the style of the existing ones: name and version,
  author, the URL you got it from, its licence and where the licence text is, whether it is
  vendored unmodified, and what the saver uses it for. Add it in the same commit as the code
  or data.

## Keeping moving

- Do not end your turn before the section 8 report. If you stop, check `git log` and your files
  for what is actually done and carry on with the next unfinished item.
- Do not deliberate for long without acting. If several minutes pass without a tool call, your
  next one writes or edits a file, or builds.
- Do not investigate for long without changing anything. If your files have not changed for a
  long stretch, make the smallest edit that moves forward, build it, check a capture, and commit.
