/* myrpc_log.c -- logging to syslog or to a plain file.
   Copyright (C) 2026 myRPC project.  */

#include "myrpc_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

static enum myrpc_log_target log_target = MYRPC_LOG_STDERR;
static FILE *log_stream = NULL;
static char log_ident[64] = "myRPC";

/* Write one message to the selected destination.  PRIORITY uses the
   syslog(3) levels.  */
static void
log_write (int priority, const char *format, va_list args)
{
  if (log_target == MYRPC_LOG_SYSLOG)
    {
      vsyslog (priority, format, args);
      return;
    }

  {
    FILE *stream = log_stream != NULL ? log_stream : stderr;
    time_t now = time (NULL);
    struct tm broken_down;
    char stamp[32];
    const char *level;

    localtime_r (&now, &broken_down);
    strftime (stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", &broken_down);

    switch (priority)
      {
      case LOG_ERR:
        level = "ERROR";
        break;
      case LOG_WARNING:
        level = "WARN";
        break;
      default:
        level = "INFO";
        break;
      }

    fprintf (stream, "%s %s[%ld]: %s: ", stamp, log_ident,
             (long) getpid (), level);
    vfprintf (stream, format, args);
    fputc ('\n', stream);
    fflush (stream);
  }
}

void
myrpc_log_open (const char *ident, enum myrpc_log_target target,
                const char *file_name)
{
  if (ident != NULL)
    {
      strncpy (log_ident, ident, sizeof log_ident - 1);
      log_ident[sizeof log_ident - 1] = '\0';
    }

  log_target = target;

  if (target == MYRPC_LOG_SYSLOG)
    {
      openlog (log_ident, LOG_PID | LOG_CONS, LOG_DAEMON);
      return;
    }

  if (target == MYRPC_LOG_FILE && file_name != NULL)
    {
      log_stream = fopen (file_name, "a");
      if (log_stream == NULL)
        {
          fprintf (stderr, "%s: cannot open log file %s, using stderr\n",
                   log_ident, file_name);
          log_target = MYRPC_LOG_STDERR;
        }
    }
}

void
myrpc_log_close (void)
{
  if (log_target == MYRPC_LOG_SYSLOG)
    closelog ();

  if (log_stream != NULL)
    {
      fclose (log_stream);
      log_stream = NULL;
    }
}

void
myrpc_log_info (const char *format, ...)
{
  va_list args;

  va_start (args, format);
  log_write (LOG_INFO, format, args);
  va_end (args);
}

void
myrpc_log_warn (const char *format, ...)
{
  va_list args;

  va_start (args, format);
  log_write (LOG_WARNING, format, args);
  va_end (args);
}

void
myrpc_log_error (const char *format, ...)
{
  va_list args;

  va_start (args, format);
  log_write (LOG_ERR, format, args);
  va_end (args);
}
