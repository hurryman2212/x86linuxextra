#include "x86linux/helper.h"

#include <execinfo.h>

int log_suppress = 0;

int _log_backtrace(const char *filename, int line, const char *func) {
  void *buffer[LOG_MAX_BACKTRACE];
  int size = backtrace(buffer, LOG_MAX_BACKTRACE);

  char **strings;
  log_perror_assert(strings = backtrace_symbols(buffer, size));

  dprintf(STDERR_FILENO, _LOG_MSG(filename, line, func, "\n", "Call Trace:"));

  for (int i = 0; i < size; i++)
    dprintf(STDERR_FILENO, " %s\n", strings[i]);

  free(strings);

  return size;
}
