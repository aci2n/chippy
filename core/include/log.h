#ifndef CHIPPY_LOG_H
#define CHIPPY_LOG_H

/*
 * Debug logging to stderr. Enabled when CHIPPY_DEBUG is defined at compile time
 * (default; disable with ./configure --disable-debug).
 */
#ifdef CHIPPY_DEBUG
#define LOG_DEBUG(fmt, ...) \
  chippy_log_debug(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) ((void)0)
#endif

void chippy_log_debug(const char *file, int line, const char *func, const char *fmt, ...);

#endif /* CHIPPY_LOG_H */
