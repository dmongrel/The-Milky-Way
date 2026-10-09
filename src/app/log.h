// A trace file, because a screen saver has no console and must not show dialogs.
//
// MILKY_WAY_LOG=1 writes %TEMP%\the-milky-way-claude.log; any other non-empty value is the path.
#pragma once

namespace app {

void LogOpen();
bool LogEnabled();
void Log(const char* format, ...) __attribute__((format(printf, 1, 2)));

}  // namespace app
