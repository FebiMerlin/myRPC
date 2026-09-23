/* myrpc_common.h -- common declarations for myRPC client and server.
   Copyright (C) 2026 myRPC project.

   This file is part of myRPC.  Coding style follows the GNU Coding
   Standards, see https://www.gnu.org/prep/standards/html_node/Writing-C.html  */

#ifndef MYRPC_COMMON_H
#define MYRPC_COMMON_H

#include <stddef.h>
#include <sys/types.h>

/* Default values used when the configuration file does not define them.  */
#define MYRPC_DEFAULT_PORT 1234
#define MYRPC_DEFAULT_HOST "127.0.0.1"

/* Limits of the protocol.  A datagram must fit into a single UDP packet,
   therefore the request size is deliberately small.  */
#define MYRPC_MAX_REQUEST 8192
#define MYRPC_MAX_REPLY 65536
#define MYRPC_MAX_LOGIN 64
#define MYRPC_MAX_COMMAND 4096

/* Response codes of the protocol (see task item 12.1).  */
#define MYRPC_CODE_OK 0
#define MYRPC_CODE_ERROR 1

/* Transport used by both sides of the protocol.  */
enum myrpc_socket_type
{
  MYRPC_SOCK_STREAM = 0,        /* SOCK_STREAM, TCP  */
  MYRPC_SOCK_DGRAM = 1          /* SOCK_DGRAM, UDP  */
};

/* --- Logging (myrpc_log.c) ------------------------------------------- */

/* Destination of the log messages.  */
enum myrpc_log_target
{
  MYRPC_LOG_SYSLOG = 0,         /* syslog(3)  */
  MYRPC_LOG_FILE = 1,           /* plain file given on the command line  */
  MYRPC_LOG_STDERR = 2          /* standard error, used before daemonizing  */
};

extern void myrpc_log_open (const char *ident, enum myrpc_log_target target,
                            const char *file_name);
extern void myrpc_log_close (void);
extern void myrpc_log_info (const char *format, ...);
extern void myrpc_log_warn (const char *format, ...);
extern void myrpc_log_error (const char *format, ...);

/* --- Configuration files (myrpc_config.c) ---------------------------- */

/* Contents of /etc/myRPC/myRPC.conf.  */
struct myrpc_config
{
  int port;
  enum myrpc_socket_type socket_type;
  int daemonize;                /* run as a daemon when non-zero  */
};

extern void myrpc_config_defaults (struct myrpc_config *config);
extern int myrpc_config_load (const char *path, struct myrpc_config *config);

/* List of user names allowed to run commands, /etc/myRPC/users.conf.  */
struct myrpc_userlist
{
  char **names;
  size_t count;
};

extern int myrpc_userlist_load (const char *path,
                                struct myrpc_userlist *list);
extern int myrpc_userlist_contains (const struct myrpc_userlist *list,
                                    const char *name);
extern void myrpc_userlist_free (struct myrpc_userlist *list);

/* --- Protocol (myrpc_proto.c) ---------------------------------------- */

/* A decoded request: {"login":"user","command":"bash command"}  */
struct myrpc_request
{
  char login[MYRPC_MAX_LOGIN];
  char command[MYRPC_MAX_COMMAND];
};

/* A decoded reply: {"code":0,"result":"..."}  */
struct myrpc_reply
{
  int code;
  char *result;                 /* malloc'ed, freed by myrpc_reply_free  */
};

extern int myrpc_request_encode (const struct myrpc_request *request,
                                 char *buffer, size_t size);
extern int myrpc_request_decode (const char *text,
                                 struct myrpc_request *request);
extern char *myrpc_reply_encode (const struct myrpc_reply *reply);
extern int myrpc_reply_decode (const char *text, struct myrpc_reply *reply);
extern void myrpc_reply_free (struct myrpc_reply *reply);

/* Escape and unescape a JSON string value.  Used by the functions above and
   exposed for the unit tests.  */
extern int myrpc_json_escape (const char *input, char *output, size_t size);
extern int myrpc_json_unescape (const char *input, size_t length,
                                char *output, size_t size);

#endif /* MYRPC_COMMON_H */
