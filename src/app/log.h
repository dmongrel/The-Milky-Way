// Debug tracing to a file, enabled by the environment variable MILKY_WAY_LOG.
//
// The shipped .scr has no console and a screen saver must not pop dialogs, so this is the only
// place it can say anything. MILKY_WAY_LOG=1 writes %TEMP%\the-milky-way.log; any other value
// is taken as the path.
#ifndef MILKY_WAY_LOG_H
#define MILKY_WAY_LOG_H

namespace app {

void LogInit();
void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
bool LogEnabled();

}  // namespace app

#endif
