/* myrpc_server.c -- daemon that executes bash commands received over a
   socket from myRPC-client.
   Copyright (C) 2026 myRPC project.

   The server reads its configuration from /etc/myRPC/myRPC.conf, the list
   of the allowed users from /etc/myRPC/users.conf and serves every new
   connection in a separate process.  Signals are handled as required by
   the task: SIGINT and SIGTERM stop the daemon, SIGHUP reloads the
   configuration, SIGCHLD reaps the finished children.  */

#include "myrpc_common.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MYRPC_CONFIG_PATH "/etc/myRPC/myRPC.conf"
#define MYRPC_USERS_PATH "/etc/myRPC/users.conf"
#define MYRPC_PID_PATH "/run/myRPC-server.pid"

/* Flags set by the signal handlers.  A handler may only touch variables of
   this type, everything else is done by the main loop.  */
static volatile sig_atomic_t stop_requested = 0;
static volatile sig_atomic_t reload_requested = 0;
static volatile sig_atomic_t child_exited = 0;

/* State of the running server.  */
static struct myrpc_config config;
static struct myrpc_userlist users;
static const char *config_path = MYRPC_CONFIG_PATH;
static const char *users_path = MYRPC_USERS_PATH;
static int listen_socket = -1;

/* Identifiers of the running worker processes.  The daemon signals only
   these processes when it stops, therefore the process group of the
   shell that started it is never disturbed.  */
static pid_t *workers = NULL;
static size_t worker_count = 0;

/* Remember a new worker process.  */
static void
add_worker (pid_t pid)
{
  pid_t *grown = realloc (workers, (worker_count + 1) * sizeof *workers);

  if (grown == NULL)
    {
      myrpc_log_error ("out of memory, the worker %ld is not registered",
                       (long) pid);
      return;
    }

  workers = grown;
  workers[worker_count++] = pid;
}

/* Forget a worker process that has already finished.  */
static void
remove_worker (pid_t pid)
{
  size_t i;

  for (i = 0; i < worker_count; i++)
    if (workers[i] == pid)
      {
        workers[i] = workers[worker_count - 1];
        worker_count--;
        return;
      }
}

static void
handle_signal (int signal_number)
{
  switch (signal_number)
    {
    case SIGINT:
    case SIGTERM:
      stop_requested = signal_number;
      break;
    case SIGHUP:
      reload_requested = 1;
      break;
    case SIGCHLD:
      child_exited = 1;
      break;
    default:
      break;
    }
}

/* Install the handlers with sigaction(2); signal(2) is not portable
   enough for a daemon.  */
static void
install_signal_handlers (void)
{
  struct sigaction action;

  memset (&action, 0, sizeof action);
  action.sa_handler = handle_signal;
  sigemptyset (&action.sa_mask);

  /* SA_RESTART is deliberately not used: accept(2) and recvfrom(2) must
     return EINTR so that the main loop can apply a new configuration or
     stop without waiting for the next client.  */
  action.sa_flags = 0;

  sigaction (SIGINT, &action, NULL);
  sigaction (SIGTERM, &action, NULL);
  sigaction (SIGHUP, &action, NULL);
  sigaction (SIGCHLD, &action, NULL);

  /* A client may close the connection before the answer is written; the
     resulting SIGPIPE must not kill the server.  */
  signal (SIGPIPE, SIG_IGN);
}

/* Turn the process into a daemon: detach from the terminal, become a
   session leader and close the standard descriptors.  */
static int
daemonize (void)
{
  pid_t pid;
  int fd;

  pid = fork ();
  if (pid < 0)
    return -1;
  if (pid > 0)
    _exit (EXIT_SUCCESS);

  if (setsid () < 0)
    return -1;

  /* The second fork guarantees that the daemon can never acquire a
     controlling terminal again.  */
  pid = fork ();
  if (pid < 0)
    return -1;
  if (pid > 0)
    _exit (EXIT_SUCCESS);

  umask (027);

  if (chdir ("/") != 0)
    return -1;

  fd = open ("/dev/null", O_RDWR);
  if (fd >= 0)
    {
      dup2 (fd, STDIN_FILENO);
      dup2 (fd, STDOUT_FILENO);
      dup2 (fd, STDERR_FILENO);
      if (fd > STDERR_FILENO)
        close (fd);
    }

  return 0;
}

/* Write the process identifier so that the service manager and the
   administrator can find the daemon.  */
static void
write_pid_file (void)
{
  FILE *stream = fopen (MYRPC_PID_PATH, "w");

  if (stream == NULL)
    {
      myrpc_log_warn ("cannot write %s: %s", MYRPC_PID_PATH,
                      strerror (errno));
      return;
    }

  fprintf (stream, "%ld\n", (long) getpid ());
  fclose (stream);
}

/* Read the whole content of STREAM into a newly allocated buffer.  */
static char *
read_stream (FILE *stream)
{
  size_t capacity = 4096;
  size_t used = 0;
  char *buffer = malloc (capacity);

  if (buffer == NULL)
    return NULL;

  for (;;)
    {
      size_t space = capacity - used - 1;
      size_t got;

      if (space == 0)
        {
          char *grown;

          capacity *= 2;
          grown = realloc (buffer, capacity);
          if (grown == NULL)
            {
              free (buffer);
              return NULL;
            }

          buffer = grown;
          space = capacity - used - 1;
        }

      got = fread (buffer + used, 1, space, stream);
      used += got;

      if (got < space)
        break;
    }

  buffer[used] = '\0';
  return buffer;
}

/* Execute COMMAND with /bin/sh, collecting the standard output and the
   standard error in the temporary files required by the task.  The
   caller owns the returned string.  */
static char *
execute_command (const char *command, int *code)
{
  char out_template[] = "/tmp/myRPC_XXXXXX.stdout";
  char err_template[] = "/tmp/myRPC_XXXXXX.stderr";
  int out_fd;
  int err_fd;
  pid_t pid;
  int status = 0;
  char *result = NULL;
  FILE *stream;

  /* mkstemps keeps the suffix required by the task.  */
  out_fd = mkstemps (out_template, strlen (".stdout"));
  err_fd = mkstemps (err_template, strlen (".stderr"));

  if (out_fd < 0 || err_fd < 0)
    {
      myrpc_log_error ("cannot create temporary files: %s",
                       strerror (errno));
      *code = MYRPC_CODE_ERROR;
      if (out_fd >= 0)
        close (out_fd);
      if (err_fd >= 0)
        close (err_fd);
      return strdup ("cannot create temporary files");
    }

  myrpc_log_info ("executing command, stdout=%s stderr=%s", out_template,
                  err_template);

  pid = fork ();
  if (pid < 0)
    {
      myrpc_log_error ("fork failed: %s", strerror (errno));
      close (out_fd);
      close (err_fd);
      unlink (out_template);
      unlink (err_template);
      *code = MYRPC_CODE_ERROR;
      return strdup ("fork failed");
    }

  if (pid == 0)
    {
      /* Child: redirect the standard streams into the temporary files and
         run the command.  */
      dup2 (out_fd, STDOUT_FILENO);
      dup2 (err_fd, STDERR_FILENO);
      close (out_fd);
      close (err_fd);

      execl ("/bin/sh", "sh", "-c", command, (char *) NULL);
      _exit (127);
    }

  while (waitpid (pid, &status, 0) < 0 && errno == EINTR)
    continue;

  lseek (out_fd, 0, SEEK_SET);
  lseek (err_fd, 0, SEEK_SET);

  if (WIFEXITED (status) && WEXITSTATUS (status) == 0)
    {
      *code = MYRPC_CODE_OK;
      stream = fdopen (dup (out_fd), "r");
    }
  else
    {
      *code = MYRPC_CODE_ERROR;
      stream = fdopen (dup (err_fd), "r");
    }

  if (stream != NULL)
    {
      result = read_stream (stream);
      fclose (stream);
    }

  if (result == NULL)
    result = strdup ("");

  /* When a command fails without writing anything to stderr the client
     still needs a description.  */
  if (*code == MYRPC_CODE_ERROR && result[0] == '\0')
    {
      char text[128];

      snprintf (text, sizeof text, "command exited with status %d",
                WIFEXITED (status) ? WEXITSTATUS (status) : -1);
      free (result);
      result = strdup (text);
    }

  close (out_fd);
  close (err_fd);
  unlink (out_template);
  unlink (err_template);

  return result;
}

/* Build the answer for one request.  The caller owns the returned
   string.  */
static char *
process_request (const char *text)
{
  struct myrpc_request request;
  struct myrpc_reply reply;
  char *message;

  reply.code = MYRPC_CODE_ERROR;
  reply.result = NULL;

  if (myrpc_request_decode (text, &request) != 0)
    {
      myrpc_log_warn ("malformed request rejected");
      reply.result = strdup ("malformed request");
    }
  else if (!myrpc_userlist_contains (&users, request.login))
    {
      myrpc_log_warn ("user '%s' is not allowed, command rejected",
                      request.login);
      reply.result = strdup ("user is not allowed to run commands");
    }
  else
    {
      myrpc_log_info ("user '%s' runs: %s", request.login, request.command);
      reply.result = execute_command (request.command, &reply.code);
    }

  message = myrpc_reply_encode (&reply);
  free (reply.result);

  return message;
}

/* Serve one accepted stream connection.  Runs in a child process.  */
static void
serve_stream_client (int client_socket)
{
  char buffer[MYRPC_MAX_REQUEST];
  ssize_t received;
  char *answer;

  received = recv (client_socket, buffer, sizeof buffer - 1, 0);
  if (received <= 0)
    {
      myrpc_log_warn ("client closed the connection before sending data");
      close (client_socket);
      return;
    }

  buffer[received] = '\0';
  answer = process_request (buffer);

  if (answer != NULL)
    {
      size_t length = strlen (answer);
      size_t sent = 0;

      while (sent < length)
        {
          ssize_t written = send (client_socket, answer + sent,
                                  length - sent, 0);

          if (written <= 0)
            {
              myrpc_log_warn ("cannot send the answer: %s",
                              strerror (errno));
              break;
            }

          sent += (size_t) written;
        }

      free (answer);
    }

  close (client_socket);
}

/* Create the listening socket according to the configuration.  */
static int
create_socket (void)
{
  int fd;
  int reuse = 1;
  struct sockaddr_in address;
  int type = (config.socket_type == MYRPC_SOCK_STREAM
              ? SOCK_STREAM : SOCK_DGRAM);

  fd = socket (AF_INET, type, 0);
  if (fd < 0)
    {
      myrpc_log_error ("cannot create socket: %s", strerror (errno));
      return -1;
    }

  setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);

  memset (&address, 0, sizeof address);
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl (INADDR_ANY);
  address.sin_port = htons ((unsigned short) config.port);

  if (bind (fd, (struct sockaddr *) &address, sizeof address) < 0)
    {
      myrpc_log_error ("cannot bind port %d: %s", config.port,
                       strerror (errno));
      close (fd);
      return -1;
    }

  if (type == SOCK_STREAM && listen (fd, SOMAXCONN) < 0)
    {
      myrpc_log_error ("cannot listen on port %d: %s", config.port,
                       strerror (errno));
      close (fd);
      return -1;
    }

  myrpc_log_info ("listening on port %d (%s socket)", config.port,
                  type == SOCK_STREAM ? "stream" : "dgram");

  return fd;
}

/* Reload both configuration files after SIGHUP.  The listening socket is
   recreated when the port or the socket type changed.  */
static void
reload_configuration (void)
{
  struct myrpc_config previous = config;

  myrpc_log_info ("SIGHUP received, reloading the configuration");

  myrpc_userlist_free (&users);
  myrpc_config_defaults (&config);
  myrpc_config_load (config_path, &config);
  myrpc_userlist_load (users_path, &users);

  if (previous.port != config.port
      || previous.socket_type != config.socket_type)
    {
      if (listen_socket >= 0)
        close (listen_socket);

      listen_socket = create_socket ();
      if (listen_socket < 0)
        {
          myrpc_log_error ("cannot apply the new configuration, stopping");
          stop_requested = SIGTERM;
        }
    }
}

/* Wait for the children that already finished.  */
static void
reap_children (void)
{
  int status;
  pid_t pid;

  while ((pid = waitpid (-1, &status, WNOHANG)) > 0)
    {
      remove_worker (pid);
      myrpc_log_info ("worker process %ld finished with status %d",
                      (long) pid, WIFEXITED (status)
                      ? WEXITSTATUS (status) : -1);
    }
}

/* Main loop of the server.  The transport is examined on every iteration,
   therefore SIGHUP may switch the server between the stream and the
   datagram socket without a restart.  Every request is served by its own
   process.  */
static void
run_server (void)
{
  while (!stop_requested)
    {
      if (reload_requested)
        {
          reload_requested = 0;
          reload_configuration ();
          continue;
        }

      if (child_exited)
        {
          child_exited = 0;
          reap_children ();
        }

      if (config.socket_type == MYRPC_SOCK_STREAM)
        {
          struct sockaddr_in peer;
          socklen_t peer_length = sizeof peer;
          int client_socket;
          pid_t pid;

          client_socket = accept (listen_socket, (struct sockaddr *) &peer,
                                  &peer_length);
          if (client_socket < 0)
            {
              if (errno == EINTR)
                continue;

              myrpc_log_error ("accept failed: %s", strerror (errno));
              continue;
            }

          pid = fork ();
          if (pid < 0)
            {
              myrpc_log_error ("cannot create a worker process: %s",
                               strerror (errno));
              close (client_socket);
              continue;
            }

          if (pid == 0)
            {
              close (listen_socket);
              serve_stream_client (client_socket);
              _exit (EXIT_SUCCESS);
            }

          add_worker (pid);
          close (client_socket);
        }
      else
        {
          char buffer[MYRPC_MAX_REQUEST];
          struct sockaddr_in peer;
          socklen_t peer_length = sizeof peer;
          ssize_t received;
          pid_t pid;

          received = recvfrom (listen_socket, buffer, sizeof buffer - 1, 0,
                               (struct sockaddr *) &peer, &peer_length);
          if (received < 0)
            {
              if (errno == EINTR)
                continue;

              myrpc_log_error ("recvfrom failed: %s", strerror (errno));
              continue;
            }

          buffer[received] = '\0';

          pid = fork ();
          if (pid < 0)
            {
              myrpc_log_error ("cannot create a worker process: %s",
                               strerror (errno));
              continue;
            }

          if (pid == 0)
            {
              char *answer = process_request (buffer);

              if (answer != NULL)
                {
                  sendto (listen_socket, answer, strlen (answer), 0,
                          (struct sockaddr *) &peer, peer_length);
                  free (answer);
                }

              _exit (EXIT_SUCCESS);
            }

          add_worker (pid);
        }
    }
}

/* Stop every worker process before the daemon itself exits, as required
   by the task.  Only the registered children are signalled.  */
static void
shutdown_workers (void)
{
  size_t i;

  if (worker_count == 0)
    {
      myrpc_log_info ("no worker process is running");
      free (workers);
      workers = NULL;
      return;
    }

  myrpc_log_info ("stopping the worker processes (%zu running)",
                  worker_count);

  for (i = 0; i < worker_count; i++)
    kill (workers[i], SIGTERM);

  for (i = 0; i < worker_count; i++)
    {
      int status;

      while (waitpid (workers[i], &status, 0) < 0 && errno == EINTR)
        continue;

      myrpc_log_info ("worker process %ld stopped", (long) workers[i]);
    }

  free (workers);
  workers = NULL;
  worker_count = 0;
}

static void
print_usage (const char *program)
{
  printf ("Usage: %s [OPTION]...\n", program);
  printf ("Daemon executing bash commands received from myRPC-client.\n\n");
  printf ("  -c, --config FILE   configuration file "
          "(default %s)\n", MYRPC_CONFIG_PATH);
  printf ("  -u, --users FILE    list of allowed users "
          "(default %s)\n", MYRPC_USERS_PATH);
  printf ("  -l, --log FILE      write the journal to FILE instead of "
          "syslog\n");
  printf ("  -f, --foreground    stay in the foreground, do not "
          "daemonize\n");
  printf ("      --help          display this help and exit\n");
}

int
main (int argc, char **argv)
{
  static const struct option long_options[] = {
    { "config", required_argument, NULL, 'c' },
    { "users", required_argument, NULL, 'u' },
    { "log", required_argument, NULL, 'l' },
    { "foreground", no_argument, NULL, 'f' },
    { "help", no_argument, NULL, 'H' },
    { NULL, 0, NULL, 0 }
  };

  const char *log_file = NULL;
  int foreground = 0;
  int option;

  while ((option = getopt_long (argc, argv, "c:u:l:fH", long_options,
                                NULL)) != -1)
    {
      switch (option)
        {
        case 'c':
          config_path = optarg;
          break;
        case 'u':
          users_path = optarg;
          break;
        case 'l':
          log_file = optarg;
          break;
        case 'f':
          foreground = 1;
          break;
        case 'H':
          print_usage (argv[0]);
          return EXIT_SUCCESS;
        default:
          print_usage (argv[0]);
          return EXIT_FAILURE;
        }
    }

  myrpc_log_open ("myRPC-server",
                  log_file != NULL ? MYRPC_LOG_FILE : MYRPC_LOG_STDERR,
                  log_file);

  myrpc_config_defaults (&config);
  myrpc_config_load (config_path, &config);

  if (myrpc_userlist_load (users_path, &users) != 0)
    myrpc_log_warn ("no user is allowed to run commands");

  if (foreground)
    config.daemonize = 0;

  if (config.daemonize)
    {
      if (daemonize () != 0)
        {
          myrpc_log_error ("cannot daemonize: %s", strerror (errno));
          return EXIT_FAILURE;
        }

      /* After the fork the standard error is gone, therefore the journal
         goes to syslog unless a file was requested.  */
      myrpc_log_close ();
      myrpc_log_open ("myRPC-server",
                      log_file != NULL ? MYRPC_LOG_FILE : MYRPC_LOG_SYSLOG,
                      log_file);
      write_pid_file ();
    }

  install_signal_handlers ();

  myrpc_log_info ("myRPC-server started, pid %ld", (long) getpid ());

  listen_socket = create_socket ();
  if (listen_socket < 0)
    {
      myrpc_userlist_free (&users);
      myrpc_log_close ();
      return EXIT_FAILURE;
    }

  run_server ();

  myrpc_log_info ("signal %d received, shutting down",
                  (int) stop_requested);

  shutdown_workers ();

  if (listen_socket >= 0)
    close (listen_socket);

  myrpc_userlist_free (&users);

  if (config.daemonize)
    unlink (MYRPC_PID_PATH);

  myrpc_log_info ("myRPC-server stopped");
  myrpc_log_close ();

  return EXIT_SUCCESS;
}
