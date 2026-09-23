/* myrpc_client.c -- console utility sending a bash command to
   myRPC-server.
   Copyright (C) 2026 myRPC project.

   The utility takes the command and the connection parameters from the
   command line, adds the name of the user it runs as, sends the request
   over a stream or a datagram socket and prints the answer of the
   server.  */

#include "myrpc_common.h"

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <netinet/in.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* The client waits at most this number of seconds for the answer of the
   server.  Without a timeout a lost datagram would block the utility
   forever.  */
#define MYRPC_TIMEOUT_SECONDS 10

/* Return the name of the user the utility runs as.  */
static const char *
current_user_name (void)
{
  const struct passwd *entry = getpwuid (getuid ());

  if (entry != NULL && entry->pw_name != NULL)
    return entry->pw_name;

  return "unknown";
}

/* Connect to HOST:PORT.  TYPE selects the transport.  The datagram
   socket is connected as well, so that send(2) and recv(2) can be used
   for both transports.  */
static int
connect_to_server (const char *host, int port, enum myrpc_socket_type type)
{
  int fd;
  struct sockaddr_in address;
  struct timeval timeout;

  fd = socket (AF_INET, type == MYRPC_SOCK_STREAM ? SOCK_STREAM : SOCK_DGRAM,
               0);
  if (fd < 0)
    {
      myrpc_log_error ("cannot create socket: %s", strerror (errno));
      return -1;
    }

  timeout.tv_sec = MYRPC_TIMEOUT_SECONDS;
  timeout.tv_usec = 0;
  setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
  setsockopt (fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);

  memset (&address, 0, sizeof address);
  address.sin_family = AF_INET;
  address.sin_port = htons ((unsigned short) port);

  if (inet_pton (AF_INET, host, &address.sin_addr) != 1)
    {
      myrpc_log_error ("invalid server address '%s'", host);
      close (fd);
      return -1;
    }

  if (connect (fd, (struct sockaddr *) &address, sizeof address) < 0)
    {
      myrpc_log_error ("cannot connect to %s:%d: %s", host, port,
                       strerror (errno));
      close (fd);
      return -1;
    }

  return fd;
}

static void
print_usage (const char *program)
{
  printf ("Usage: %s -c COMMAND [OPTION]...\n", program);
  printf ("Run a bash command on a remote host through myRPC-server.\n\n");
  printf ("  -c, --command COMMAND  bash command to execute\n");
  printf ("  -h, --host ADDRESS     address of the server "
          "(default %s)\n", MYRPC_DEFAULT_HOST);
  printf ("  -p, --port PORT        port of the server "
          "(default %d)\n", MYRPC_DEFAULT_PORT);
  printf ("  -s, --stream           use a stream socket (default)\n");
  printf ("  -d, --dgram            use a datagram socket\n");
  printf ("  -l, --log FILE         write the journal to FILE instead of "
          "syslog\n");
  printf ("      --help             display this help and exit\n\n");
  printf ("Example:\n");
  printf ("  %s -h 192.168.1.10 -p 1234 -s -c \"ls -la /etc\"\n", program);
}

int
main (int argc, char **argv)
{
  static const struct option long_options[] = {
    { "command", required_argument, NULL, 'c' },
    { "host", required_argument, NULL, 'h' },
    { "port", required_argument, NULL, 'p' },
    { "stream", no_argument, NULL, 's' },
    { "dgram", no_argument, NULL, 'd' },
    { "log", required_argument, NULL, 'l' },
    { "help", no_argument, NULL, 'H' },
    { NULL, 0, NULL, 0 }
  };

  const char *command = NULL;
  const char *host = MYRPC_DEFAULT_HOST;
  const char *log_file = NULL;
  int port = MYRPC_DEFAULT_PORT;
  enum myrpc_socket_type socket_type = MYRPC_SOCK_STREAM;
  int option;
  int fd;
  struct myrpc_request request;
  struct myrpc_reply reply;
  char buffer[MYRPC_MAX_REQUEST];
  char answer[MYRPC_MAX_REPLY];
  ssize_t received;
  int length;
  int status = EXIT_SUCCESS;

  while ((option = getopt_long (argc, argv, "c:h:p:sdl:H", long_options,
                                NULL)) != -1)
    {
      switch (option)
        {
        case 'c':
          command = optarg;
          break;
        case 'h':
          host = optarg;
          break;
        case 'p':
          port = atoi (optarg);
          break;
        case 's':
          socket_type = MYRPC_SOCK_STREAM;
          break;
        case 'd':
          socket_type = MYRPC_SOCK_DGRAM;
          break;
        case 'l':
          log_file = optarg;
          break;
        case 'H':
          print_usage (argv[0]);
          return EXIT_SUCCESS;
        default:
          print_usage (argv[0]);
          return EXIT_FAILURE;
        }
    }

  myrpc_log_open ("myRPC-client",
                  log_file != NULL ? MYRPC_LOG_FILE : MYRPC_LOG_SYSLOG,
                  log_file);

  if (command == NULL)
    {
      fprintf (stderr, "%s: no command given\n", argv[0]);
      print_usage (argv[0]);
      myrpc_log_close ();
      return EXIT_FAILURE;
    }

  if (port <= 0 || port > 65535)
    {
      fprintf (stderr, "%s: invalid port %d\n", argv[0], port);
      myrpc_log_close ();
      return EXIT_FAILURE;
    }

  memset (&request, 0, sizeof request);
  strncpy (request.login, current_user_name (), sizeof request.login - 1);
  strncpy (request.command, command, sizeof request.command - 1);

  length = myrpc_request_encode (&request, buffer, sizeof buffer);
  if (length < 0)
    {
      fprintf (stderr, "%s: the command is too long\n", argv[0]);
      myrpc_log_error ("the command of user '%s' does not fit into the "
                       "request", request.login);
      myrpc_log_close ();
      return EXIT_FAILURE;
    }

  myrpc_log_info ("user '%s' sends a command to %s:%d (%s socket)",
                  request.login, host, port,
                  socket_type == MYRPC_SOCK_STREAM ? "stream" : "dgram");

  fd = connect_to_server (host, port, socket_type);
  if (fd < 0)
    {
      fprintf (stderr, "%s: cannot connect to %s:%d\n", argv[0], host, port);
      myrpc_log_close ();
      return EXIT_FAILURE;
    }

  if (send (fd, buffer, (size_t) length, 0) != length)
    {
      fprintf (stderr, "%s: cannot send the request: %s\n", argv[0],
               strerror (errno));
      myrpc_log_error ("cannot send the request: %s", strerror (errno));
      close (fd);
      myrpc_log_close ();
      return EXIT_FAILURE;
    }

  received = recv (fd, answer, sizeof answer - 1, 0);
  if (received <= 0)
    {
      fprintf (stderr, "%s: no answer from the server: %s\n", argv[0],
               received == 0 ? "connection closed" : strerror (errno));
      myrpc_log_error ("no answer from %s:%d", host, port);
      close (fd);
      myrpc_log_close ();
      return EXIT_FAILURE;
    }

  answer[received] = '\0';
  close (fd);

  if (myrpc_reply_decode (answer, &reply) != 0)
    {
      fprintf (stderr, "%s: malformed answer from the server\n", argv[0]);
      myrpc_log_error ("malformed answer from %s:%d", host, port);
      myrpc_log_close ();
      return EXIT_FAILURE;
    }

  /* The output of a successful command goes to the standard output, the
     description of an error goes to the standard error, so that the
     utility can be used in shell pipelines.  */
  if (reply.code == MYRPC_CODE_OK)
    {
      fputs (reply.result, stdout);
      if (reply.result[0] != '\0'
          && reply.result[strlen (reply.result) - 1] != '\n')
        fputc ('\n', stdout);

      myrpc_log_info ("the command finished successfully");
    }
  else
    {
      fputs (reply.result, stderr);
      if (reply.result[0] != '\0'
          && reply.result[strlen (reply.result) - 1] != '\n')
        fputc ('\n', stderr);

      myrpc_log_warn ("the server answered with code %d", reply.code);
      status = EXIT_FAILURE;
    }

  myrpc_reply_free (&reply);
  myrpc_log_close ();

  return status;
}
