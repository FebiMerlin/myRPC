/* myrpc_proto.c -- JSON protocol of myRPC.
   Copyright (C) 2026 myRPC project.

   The protocol carries two messages (see task item 12.1):

     request: {"login":"user","command":"bash command"}
     reply:   {"code":0,"result":"text"}

   The parser is written by hand so that the programs do not depend on any
   external library.  Only the subset of JSON needed by the protocol is
   supported: a flat object whose values are strings or integers.  */

#include "myrpc_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Escape the characters that may not appear literally inside a JSON string
   and store the result in OUTPUT.  Return 0 on success, -1 if OUTPUT is too
   small.  */
int
myrpc_json_escape (const char *input, char *output, size_t size)
{
  size_t used = 0;

  for (; *input != '\0'; input++)
    {
      const char *replacement = NULL;
      char buffer[8];

      switch (*input)
        {
        case '"':
          replacement = "\\\"";
          break;
        case '\\':
          replacement = "\\\\";
          break;
        case '\n':
          replacement = "\\n";
          break;
        case '\r':
          replacement = "\\r";
          break;
        case '\t':
          replacement = "\\t";
          break;
        case '\b':
          replacement = "\\b";
          break;
        case '\f':
          replacement = "\\f";
          break;
        default:
          if ((unsigned char) *input < 0x20)
            {
              snprintf (buffer, sizeof buffer, "\\u%04x",
                        (unsigned char) *input);
              replacement = buffer;
            }
          break;
        }

      if (replacement != NULL)
        {
          size_t length = strlen (replacement);

          if (used + length + 1 > size)
            return -1;

          memcpy (output + used, replacement, length);
          used += length;
        }
      else
        {
          if (used + 2 > size)
            return -1;

          output[used++] = *input;
        }
    }

  if (used + 1 > size)
    return -1;

  output[used] = '\0';
  return 0;
}

/* Reverse of myrpc_json_escape.  LENGTH is the number of characters of
   INPUT that belong to the string value.  */
int
myrpc_json_unescape (const char *input, size_t length, char *output,
                     size_t size)
{
  size_t used = 0;
  size_t i;

  for (i = 0; i < length; i++)
    {
      char c = input[i];

      if (c == '\\' && i + 1 < length)
        {
          i++;
          switch (input[i])
            {
            case 'n':
              c = '\n';
              break;
            case 'r':
              c = '\r';
              break;
            case 't':
              c = '\t';
              break;
            case 'b':
              c = '\b';
              break;
            case 'f':
              c = '\f';
              break;
            case 'u':
              /* Only the control characters produced by the escaper are
                 decoded; they always fit into one byte.  */
              if (i + 4 < length)
                {
                  char hex[5];

                  memcpy (hex, input + i + 1, 4);
                  hex[4] = '\0';
                  c = (char) strtol (hex, NULL, 16);
                  i += 4;
                }
              break;
            default:
              c = input[i];
              break;
            }
        }

      if (used + 2 > size)
        return -1;

      output[used++] = c;
    }

  if (used + 1 > size)
    return -1;

  output[used] = '\0';
  return 0;
}

/* Find the string value of KEY inside TEXT.  On success *VALUE points to
   the first character of the value and *LENGTH holds its length.  */
static int
find_string_value (const char *text, const char *key, const char **value,
                   size_t *length)
{
  char pattern[64];
  const char *position;
  const char *start;
  const char *end;

  snprintf (pattern, sizeof pattern, "\"%s\"", key);
  position = strstr (text, pattern);
  if (position == NULL)
    return -1;

  position = strchr (position + strlen (pattern), ':');
  if (position == NULL)
    return -1;

  start = strchr (position, '"');
  if (start == NULL)
    return -1;
  start++;

  /* Find the closing quote, skipping the escaped ones.  */
  for (end = start; *end != '\0'; end++)
    {
      if (*end == '\\' && end[1] != '\0')
        {
          end++;
          continue;
        }

      if (*end == '"')
        break;
    }

  if (*end != '"')
    return -1;

  *value = start;
  *length = (size_t) (end - start);
  return 0;
}

/* Find the integer value of KEY inside TEXT.  */
static int
find_int_value (const char *text, const char *key, int *value)
{
  char pattern[64];
  const char *position;

  snprintf (pattern, sizeof pattern, "\"%s\"", key);
  position = strstr (text, pattern);
  if (position == NULL)
    return -1;

  position = strchr (position + strlen (pattern), ':');
  if (position == NULL)
    return -1;

  *value = atoi (position + 1);
  return 0;
}

int
myrpc_request_encode (const struct myrpc_request *request, char *buffer,
                      size_t size)
{
  char login[MYRPC_MAX_LOGIN * 2];
  char command[MYRPC_MAX_COMMAND * 2];
  int written;

  if (myrpc_json_escape (request->login, login, sizeof login) != 0)
    return -1;

  if (myrpc_json_escape (request->command, command, sizeof command) != 0)
    return -1;

  written = snprintf (buffer, size, "{\"login\":\"%s\",\"command\":\"%s\"}",
                      login, command);

  if (written < 0 || (size_t) written >= size)
    return -1;

  return written;
}

int
myrpc_request_decode (const char *text, struct myrpc_request *request)
{
  const char *value;
  size_t length;

  memset (request, 0, sizeof *request);

  if (find_string_value (text, "login", &value, &length) != 0)
    return -1;

  if (myrpc_json_unescape (value, length, request->login,
                           sizeof request->login) != 0)
    return -1;

  if (find_string_value (text, "command", &value, &length) != 0)
    return -1;

  if (myrpc_json_unescape (value, length, request->command,
                           sizeof request->command) != 0)
    return -1;

  return 0;
}

char *
myrpc_reply_encode (const struct myrpc_reply *reply)
{
  const char *result = reply->result != NULL ? reply->result : "";
  size_t escaped_size = strlen (result) * 6 + 2;
  char *escaped;
  char *message;
  size_t message_size;

  escaped = malloc (escaped_size);
  if (escaped == NULL)
    return NULL;

  if (myrpc_json_escape (result, escaped, escaped_size) != 0)
    {
      free (escaped);
      return NULL;
    }

  message_size = strlen (escaped) + 64;
  message = malloc (message_size);
  if (message == NULL)
    {
      free (escaped);
      return NULL;
    }

  snprintf (message, message_size, "{\"code\":%d,\"result\":\"%s\"}",
            reply->code, escaped);

  free (escaped);
  return message;
}

int
myrpc_reply_decode (const char *text, struct myrpc_reply *reply)
{
  const char *value;
  size_t length;

  reply->code = MYRPC_CODE_ERROR;
  reply->result = NULL;

  if (find_int_value (text, "code", &reply->code) != 0)
    return -1;

  if (find_string_value (text, "result", &value, &length) != 0)
    return -1;

  reply->result = malloc (length + 1);
  if (reply->result == NULL)
    return -1;

  if (myrpc_json_unescape (value, length, reply->result, length + 1) != 0)
    {
      free (reply->result);
      reply->result = NULL;
      return -1;
    }

  return 0;
}

void
myrpc_reply_free (struct myrpc_reply *reply)
{
  free (reply->result);
  reply->result = NULL;
}
