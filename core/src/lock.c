#include "chippy.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if defined(_WIN32)
#define CHIPPY_HAVE_FLOCK 0
#else
#include <sys/file.h>
#define CHIPPY_HAVE_FLOCK 1
#endif

static int lock_fd = -1;
static char lock_path[CHIPPY_MAX_PATH];
static int lock_depth;

static int lock_path_for_dir(const char *dir, char *path, size_t path_cap)
{
  int n;

  n = snprintf(path, path_cap, "%s/lock", dir);
  if (n < 0 || (size_t)n >= path_cap) {
    return -1;
  }
  return 0;
}

static int lock_acquire(const char *dir, int nonblock)
{
  char path[CHIPPY_MAX_PATH];

  if (dir == NULL) {
    return -1;
  }
  if (lock_path_for_dir(dir, path, sizeof(path)) != 0) {
    return -1;
  }
  if (lock_depth > 0) {
    if (!chippy_str_eq(lock_path, path)) {
      return -1;
    }
    lock_depth++;
    return 0;
  }
#if !CHIPPY_HAVE_FLOCK
  (void)nonblock;
  return -1;
#else
  lock_fd = open(path, O_RDWR | O_CREAT, 0600);
  if (lock_fd < 0) {
    return -1;
  }
  if (nonblock) {
    if (flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
      close(lock_fd);
      lock_fd = -1;
      return -1;
    }
  } else if (flock(lock_fd, LOCK_EX) != 0) {
    close(lock_fd);
    lock_fd = -1;
    return -1;
  }
  strncpy(lock_path, path, CHIPPY_MAX_PATH - 1);
  lock_path[CHIPPY_MAX_PATH - 1] = '\0';
  lock_depth = 1;
  return 0;
#endif
}

int chippy_dir_lock(const char *dir)
{
  return lock_acquire(dir, 0);
}

int chippy_dir_trylock(const char *dir)
{
  return lock_acquire(dir, 1);
}

int chippy_dir_unlock(const char *dir)
{
  char path[CHIPPY_MAX_PATH];

  if (dir == NULL) {
    return -1;
  }
  if (lock_path_for_dir(dir, path, sizeof(path)) != 0) {
    return -1;
  }
  if (lock_depth <= 0 || lock_fd < 0 || !chippy_str_eq(lock_path, path)) {
    return -1;
  }
  lock_depth--;
  if (lock_depth > 0) {
    return 0;
  }
#if CHIPPY_HAVE_FLOCK
  flock(lock_fd, LOCK_UN);
#endif
  close(lock_fd);
  lock_fd = -1;
  lock_path[0] = '\0';
  return 0;
}
