/* test_proto.c -- unit tests of the myRPC protocol and of the
   configuration parser.
   Copyright (C) 2026 myRPC project.

   The tests do not use any framework: every check prints its result and
   increases the counter of the failures, the exit status of the program
   reports the outcome to the continuous integration.  */

#include "myrpc_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures = 0;
static int checks = 0;

static void
check (int condition, const char *description)
{
  checks++;

  if (condition)
    printf ("  ok   %s\n", description);
  else
    {
      printf ("  FAIL %s\n", description);
      failures++;
    }
}

/* The request survives encoding and decoding without changes.  */
static void
test_request_round_trip (void)
{
  struct myrpc_request original;
  struct myrpc_request decoded;
  char buffer[MYRPC_MAX_REQUEST];

  printf ("request round trip\n");

  memset (&original, 0, sizeof original);
  strcpy (original.login, "student");
  strcpy (original.command, "ls -la /etc");

  check (myrpc_request_encode (&original, buffer, sizeof buffer) > 0,
         "the request is encoded");
  check (strstr (buffer, "\"login\":\"student\"") != NULL,
         "the encoded request contains the login");
  check (myrpc_request_decode (buffer, &decoded) == 0,
         "the request is decoded");
  check (strcmp (decoded.login, original.login) == 0,
         "the login survives the round trip");
  check (strcmp (decoded.command, original.command) == 0,
         "the command survives the round trip");
}

/* Special characters must be escaped, otherwise the message breaks the
   JSON syntax (task item 12.3).  */
static void
test_special_characters (void)
{
  struct myrpc_request original;
  struct myrpc_request decoded;
  char buffer[MYRPC_MAX_REQUEST];

  printf ("special characters\n");

  memset (&original, 0, sizeof original);
  strcpy (original.login, "student");
  strcpy (original.command, "echo \"hello\" | grep 'h' ; echo c:\\path");

  check (myrpc_request_encode (&original, buffer, sizeof buffer) > 0,
         "a command with quotes is encoded");
  check (strstr (buffer, "\\\"hello\\\"") != NULL,
         "the double quotes are escaped");
  check (myrpc_request_decode (buffer, &decoded) == 0,
         "a command with quotes is decoded");
  check (strcmp (decoded.command, original.command) == 0,
         "the command with quotes survives the round trip");
}

/* A multi line answer of a command must survive the transport.  */
static void
test_reply_round_trip (void)
{
  struct myrpc_reply original;
  struct myrpc_reply decoded;
  char *message;

  printf ("reply round trip\n");

  original.code = MYRPC_CODE_OK;
  original.result = strdup ("first line\nsecond line\ttabbed\n");

  message = myrpc_reply_encode (&original);
  check (message != NULL, "the reply is encoded");

  if (message != NULL)
    {
      check (strstr (message, "\"code\":0") != NULL,
             "the encoded reply contains the code");
      check (strstr (message, "\\n") != NULL,
             "the line breaks are escaped");
      check (myrpc_reply_decode (message, &decoded) == 0,
             "the reply is decoded");
      check (decoded.code == original.code, "the code survives");
      check (strcmp (decoded.result, original.result) == 0,
             "the result survives");
      myrpc_reply_free (&decoded);
      free (message);
    }

  free (original.result);
}

/* Malformed messages must be rejected instead of crashing the server.  */
static void
test_malformed_input (void)
{
  struct myrpc_request request;
  struct myrpc_reply reply;

  printf ("malformed input\n");

  check (myrpc_request_decode ("not a json at all", &request) != 0,
         "plain text is rejected");
  check (myrpc_request_decode ("{\"login\":\"user\"}", &request) != 0,
         "a request without a command is rejected");
  check (myrpc_request_decode ("{\"command\":\"ls\"}", &request) != 0,
         "a request without a login is rejected");
  check (myrpc_reply_decode ("{\"code\":0}", &reply) != 0,
         "a reply without a result is rejected");
}

/* The parser of the configuration file must understand the example from
   the task (item 12.5) and ignore the comments.  */
static void
test_configuration (void)
{
  const char *path = "/tmp/myrpc_test.conf";
  struct myrpc_config config;
  FILE *stream;

  printf ("configuration file\n");

  stream = fopen (path, "w");
  if (stream == NULL)
    {
      printf ("  FAIL cannot create %s\n", path);
      failures++;
      return;
    }

  fputs ("#comment\n"
         "port = 4321\n"
         "#socket_type = dgram\n"
         "socket_type = stream\n", stream);
  fclose (stream);

  myrpc_config_defaults (&config);
  check (config.port == MYRPC_DEFAULT_PORT, "the default port is used");

  myrpc_config_load (path, &config);
  check (config.port == 4321, "the port is read from the file");
  check (config.socket_type == MYRPC_SOCK_STREAM,
         "the commented out socket type is ignored");

  unlink (path);
}

/* The white list decides whether a command is executed at all.  */
static void
test_user_list (void)
{
  const char *path = "/tmp/myrpc_test_users.conf";
  struct myrpc_userlist users;
  FILE *stream;

  printf ("list of allowed users\n");

  stream = fopen (path, "w");
  if (stream == NULL)
    {
      printf ("  FAIL cannot create %s\n", path);
      failures++;
      return;
    }

  fputs ("# allowed users\n"
         "student\n"
         "  admin1  \n"
         "\n", stream);
  fclose (stream);

  check (myrpc_userlist_load (path, &users) == 0, "the list is loaded");
  check (users.count == 2, "two users are loaded");
  check (myrpc_userlist_contains (&users, "student") == 1,
         "an allowed user is found");
  check (myrpc_userlist_contains (&users, "admin1") == 1,
         "the surrounding spaces are stripped");
  check (myrpc_userlist_contains (&users, "intruder") == 0,
         "an unknown user is rejected");
  check (myrpc_userlist_contains (&users, "") == 0,
         "an empty name is rejected");

  myrpc_userlist_free (&users);
  unlink (path);
}

int
main (void)
{
  myrpc_log_open ("myRPC-test", MYRPC_LOG_FILE, "/dev/null");

  printf ("=== unit tests of myRPC ===\n");

  test_request_round_trip ();
  test_special_characters ();
  test_reply_round_trip ();
  test_malformed_input ();
  test_configuration ();
  test_user_list ();

  printf ("=== %d checks, %d failure(s) ===\n", checks, failures);

  myrpc_log_close ();

  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
