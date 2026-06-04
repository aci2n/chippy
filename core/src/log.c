#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void chippy_log_debug(const char *file, int line, const char *func, const char *fmt, ...)
{
#ifdef CHIPPY_DEBUG
  const char *base;
  va_list ap;

  base = strrchr(file, '/');
  if (base != NULL) {
    base++;
  } else {
    base = file;
  }
  fprintf(stderr, "chippy DEBUG %s:%d %s: ", base, line, func);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
#else
  (void)file;
  (void)line;
  (void)func;
  (void)fmt;
#endif
}
