// Command-line parsing for the screen-saver host.
//
// Semantics match ghost-saver, nuke-saver and the-black-hole: a leading '/' or '-'
// is stripped, the first remaining character selects the mode case-insensitively, and anything
// unrecognised exits 0.
//
// Parsing is kept free of Win32 calls so it can be exercised by a test without
// creating a window. Validating that a parent HWND still exists is the caller's job.
#ifndef MILKY_WAY_ARGS_H
#define MILKY_WAY_ARGS_H

namespace app {

enum class Mode {
    FullScreen,  // no arguments, or /s
    Preview,     // /p <hwnd>, /p:<hwnd>
    Configure,   // /c, /c:<hwnd>
    Windowed,    // /w: a resizable desktop window for development; Esc closes it
    Exit,        // anything else: leave immediately, quietly, with code 0
};

struct Args {
    Mode mode = Mode::FullScreen;

    // The window handle as written on the command line, or 0 when none was given.
    // Preview REQUIRES one; Configure treats it as an optional owner for the dialog.
    unsigned long long parentHandle = 0;
    bool               hasParent    = false;
};

// argv follows the usual convention: argv[0] is the executable.
Args Parse(int argc, const wchar_t* const* argv);

}  // namespace app

#endif
