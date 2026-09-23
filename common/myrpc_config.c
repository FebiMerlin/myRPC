/* myrpc_config.c -- parser of the server configuration and of the list of
   allowed users.
   Copyright (C) 2026 myRPC project.

   Both files use the same simple syntax:

     # comment
     key = value

   Empty lines and lines starting with '#' are ignored; spaces around the
   key and the value are stripped.  */

#include "myrpc_common.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Remove leading and trailing white space from TEXT in place and return a
   pointer to the first significant character.  */
static char *
strip (char *text)
{
  char *end;

  while (*text != '\0' && isspace ((unsigned char) *text))
    text++;

  if (*text == '\0')
    return text;

  end = text + strlen (text) - 1;
  while (end > text && isspace ((unsigned char) *end))
    *end-- = '\0';

  return text;
}

/* Cut the comment off LINE.  A '#' starts a comment up to the end of the
   line.  */
static void
strip_comment (char *line)
{
  char *hash = strchr (line, '#');

  if (hash != NULL)
    *hash = '\0';
}

void
myrpc_config_defaults (struct myrpc_config *config)
{
  config->port = MYRPC_DEFAULT_PORT;
  config->socket_type = MYRPC_SOCK_STREAM;
  config->daemonize = 1;
}

int
myrpc_config_load (const char *path, struct myrpc_config *config)
{
  FILE *stream;
  char line[512];
  int line_number = 0;
  int errors = 0;

  stream = fopen (path, "r");
  if (stream == NULL)
    {
      myrpc_log_warn ("cannot open configuration file %s, using defaults",
                      path);
      return -1;
    }

  while (fgets (line, sizeof line, stream) != NULL)
    {
      char *key;
      char *value;
      char *separator;

      line_number++;
      strip_comment (line);

      key = strip (line);
      if (*key == '\0')
        continue;

      separator = strchr (key, '=');
      if (separator == NULL)
        {
          myrpc_log_warn ("%s:%d: line without '=' ignored", path,
                          line_number);
          errors++;
          continue;
        }

      *separator = '\0';
      value = strip (separator + 1);
      key = strip (key);

      if (strcmp (key, "port") == 0)
        {
          int port = atoi (value);

          if (port <= 0 || port > 65535)
            {
              myrpc_log_warn ("%s:%d: invalid port '%s', keeping %d", path,
                              line_number, value, config->port);
              errors++;
            }
          else
            config->port = port;
        }
      else if (strcmp (key, "socket_type") == 0)
        {
          if (strcmp (value, "stream") == 0)
            config->socket_type = MYRPC_SOCK_STREAM;
          else if (strcmp (value, "dgram") == 0)
            config->socket_type = MYRPC_SOCK_DGRAM;
          else
            {
              myrpc_log_warn ("%s:%d: unknown socket type '%s'", path,
                              line_number, value);
              errors++;
            }
        }
      else if (strcmp (key, "daemon") == 0)
        {
          config->daemonize = (strcmp (value, "yes") == 0
                               || strcmp (value, "1") == 0);
        }
      else
        {
          myrpc_log_warn ("%s:%d: unknown key '%s' ignored", path,
                          line_number, key);
          errors++;
        }
    }

  fclose (stream);

  myrpc_log_info ("configuration loaded from %s: port=%d socket_type=%s",
                  path, config->port,
                  config->socket_type == MYRPC_SOCK_STREAM
                  ? "stream" : "dgram");

  return errors == 0 ? 0 : 1;
}

int
myrpc_userlist_load (const char *path, struct myrpc_userlist *list)
{
  FILE *stream;
  char line[256];
  char **names = NULL;
  size_t count = 0;

  list->names = NULL;
  list->count = 0;

  stream = fopen (path, "r");
  if (stream == NULL)
    {
      myrpc_log_error ("cannot open list of users %s", path);
      return -1;
    }

  while (fgets (line, sizeof line, stream) != NULL)
    {
      char *name;
      char **grown;

      strip_comment (line);
      name = strip (line);
      if (*name == '\0')
        continue;

      grown = realloc (names, (count + 1) * sizeof *names);
      if (grown == NULL)
        {
          myrpc_log_error ("out of memory while reading %s", path);
          break;
        }

      names = grown;
      names[count] = strdup (name);
      if (names[count] == NULL)
        {
          myrpc_log_error ("out of memory while reading %s", path);
          break;
        }

      count++;
    }

  fclose (stream);

  list->names = names;
  list->count = count;

  myrpc_log_info ("%zu allowed user(s) loaded from %s", count, path);

  return 0;
}

int
myrpc_userlist_contains (const struct myrpc_userlist *list, const char *name)
{
  size_t i;

  if (name == NULL || *name == '\0')
    return 0;

  for (i = 0; i < list->count; i++)
    if (strcmp (list->names[i], name) == 0)
      return 1;

  return 0;
}

void
myrpc_userlist_free (struct myrpc_userlist *list)
{
  size_t i;

  for (i = 0; i < list->count; i++)
    free (list->names[i]);

  free (list->names);
  list->names = NULL;
  list->count = 0;
}
