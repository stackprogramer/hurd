/* 
   Copyright (C) 1996 Free Software Foundation, Inc.
   Written by Michael I. Bushnell, p/BSG.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#include <hurd/netfs.h>
#include <sys/socket.h>
#include <stdio.h>
#include <device/device.h>
#include "nfs.h"
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <maptime.h>
#include <argp.h>
#include <argz.h>

extern char *localhost ();

/* Default number of times to retry RPCs when mounted soft. */
#define DEFAULT_SOFT_RETRIES  3	

/* Default number of seconds to timeout cached stat information. */
#define DEFAULT_STAT_TIMEOUT  3

/* Default number of seconds to timeout cached file contents. */
#define DEFAULT_CACHE_TIMEOUT 3	

/* Default maximum number of bytes to read at once. */
#define DEFAULT_READ_SIZE     8192 

/* Default maximum number of bytes to write at once. */
#define DEFAULT_WRITE_SIZE    8192 


/* Number of seconds to timeout cached stat information. */
int stat_timeout = DEFAULT_STAT_TIMEOUT;

/* Number of seconds to timeout cached file contents. */
int cache_timeout = DEFAULT_CACHE_TIMEOUT;

/* Number of seconds to wait for first retransmission of an RPC. */
int initial_transmit_timeout = 1;

/* Maximum number of seconds to wait between retransmission of RPCs. */
int max_transmit_timeout = 30;

/* Maximum number of retries to send when mounted soft. */
int soft_retries = DEFAULT_SOFT_RETRIES;

/* True iff we are mounted soft. */
int mounted_soft = 1;

/* Maximum number of bytes to read at once. */
int read_size = DEFAULT_READ_SIZE;

/* Maximum number of bytes to write at once. */
int write_size = DEFAULT_WRITE_SIZE;

#define OPT_SOFT	's'
#define OPT_HARD	'h'
#define OPT_RSIZE	'r'
#define OPT_WSIZE	'w'
#define OPT_STAT_TO	-2
#define OPT_CACHE_TO	-3
#define OPT_INIT_TR_TO	-4
#define OPT_MAX_TR_TO	-5
#define OPT_MNT_PORT    -6
#define OPT_MNT_PORT_D  -7
#define OPT_NFS_PORT    -8
#define OPT_NFS_PORT_D  -9
#define OPT_HOLD	-10
#define OPT_MNT_PROG    -11
#define OPT_NFS_PROG    -12
#define OPT_PMAP_PORT	-13

/* Return a string corresponding to the printed rep of DEFAULT_what */
#define ___D(what) #what
#define __D(what) ___D(what)
#define _D(what) __D(DEFAULT_ ## what)

/* Options usable both at startup and at runtime.  */
static const struct argp_option common_options[] =
{
  {0,0,0,0,0,1},
  {"soft",		    OPT_SOFT,	   "RETRIES", OPTION_ARG_OPTIONAL,
     "File system requests will eventually fail, after RETRIES tries if"
     " specified, otherwise " _D(SOFT_RETRIES)},
  {"hard",		    OPT_HARD, 0, 0,
     "Retry file systems requests until they succeed"},

  {0,0,0,0,0,2},
  {"read-size",		    OPT_RSIZE,	   "BYTES", 0,
     "Max packet size for reads (default " _D(READ_SIZE) ")"},
  {"rsize",0,0,OPTION_ALIAS},
  {"write-size",	    OPT_WSIZE,	   "BYTES", 0,
     "Max packet size for writes (default " _D(WRITE_SIZE)")"},
  {"wsize",0,0,OPTION_ALIAS},

  {0,0,0,0,"Timeouts:",3},
  {"stat-timeout",	    OPT_STAT_TO,   "SEC", 0,
     "Timeout for cached stat information (default " _D(STAT_TIMEOUT) ")"},
  {"cache-timeout",	    OPT_CACHE_TO,  "SEC", 0,
     "Timeout for cached file data (default " _D(CACHE_TIMEOUT) ")"},
  {"init-transmit-timeout", OPT_INIT_TR_TO,"SEC", 0}, 
  {"max-transmit-timeout",  OPT_MAX_TR_TO, "SEC", 0}, 

  {0}
};

static error_t
parse_common_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case OPT_SOFT:
      mounted_soft = 1;
      if (arg)
	soft_retries = atoi (arg);
      break;
    case OPT_HARD:
      mounted_soft = 0;
      break;

    case OPT_RSIZE: read_size = atoi (arg); break;
    case OPT_WSIZE: write_size = atoi (arg); break;

    case OPT_STAT_TO: stat_timeout = atoi (arg); break;
    case OPT_CACHE_TO: cache_timeout = atoi (arg); break;
    case OPT_INIT_TR_TO: initial_transmit_timeout = atoi (arg); break;
    case OPT_MAX_TR_TO: max_transmit_timeout = atoi (arg); break;

    default:
      return EINVAL;
    }
  return 0;
}

/* Options usable only at startup.  */
static const struct argp_option startup_options[] = {
  {0,0,0,0,"Server specification:",10},
  {"mount-port",	    OPT_MNT_PORT,  "PORT", 0,
     "Port for mount server"},
  {"default-mount-port",    OPT_MNT_PORT_D,"PORT", 0,
     "Port for mount server, if none can be found automatically"},
  {"mount-program",	    OPT_MNT_PROG,  "ID[.VERS]"},

  {"nfs-port",	            OPT_NFS_PORT,  "PORT", 0,
     "Port for nfs operations"},
  {"default-nfs-port",      OPT_NFS_PORT_D,"PORT", 0,
     "Port for nfs operations, if none can be found automatically"},
  {"nfs-program",           OPT_NFS_PROG,  "ID[.VERS]"},

  {"pmap-port",             OPT_PMAP_PORT,  "SVC|PORT"},

  {"hold", OPT_HOLD, 0, OPTION_HIDDEN}, /*  */
  { 0 }
};
static char *args_doc = "REMOTE_FS [HOST]";
static char *doc = "If HOST is not specified, an attempt is made to extract"
" it from REMOTE_FS, using either the `HOST:FS' or `FS@HOST' notations.";

/* Called when the the filesystem receives a set-options request.  ARGC and
   ARGV are the arguments given, and STANDARD_ARGP is a pointer to a struct
   argp containing the info necessary to parse `standard' netfs runtime
   options.  The user may chain this onto the end of his own argp structure
   and call argp_parse, or ignore it completely (or indeed, just call
   argp_parse on it -- which is the behavior of the default implementation of
   this function.  EINVAL is returned if an unknown option is encountered.  */
error_t
netfs_parse_runtime_options (int argc, char **argv,
			     const struct argp *standard_argp)
{
  const struct argp *argp_parents[] = { standard_argp, 0 };
  struct argp argp =
    { common_options, parse_common_opt, 0, 0, argp_parents };

  return
    argp_parse (&argp, argc, argv,
		ARGP_NO_ERRS | ARGP_NO_HELP | ARGP_PARSE_ARGV0,
		0, 0);
}

/* Called when the the filesystem receives a get-options request.  ARGZ &
   ARGZ_LEN will contain information on `standard' netfs options; the user
   may extend them (probably by using argz_add), or ignore them, in which
   case case ARGZ should be freed, as it is malloced.  The default
   implementation simply leaves ARGZ & ARGZ_LEN unmodified and returns sucess
   (0).  */
error_t
netfs_unparse_runtime_options (char **argz, size_t *argz_len)
{
  char buf[80];
  error_t err = 0;

#define FOPT(fmt, arg) \
  do { \
    if (! err) \
      { \
	snprintf (buf, sizeof buf, fmt, arg); \
	err = argz_add (argz, argz_len, buf); \
      } \
  } while (0)

  if (mounted_soft)
    FOPT ("--soft=%d", soft_retries);
  else
    err = argz_add (argz, argz_len, "--hard");

  FOPT ("--read-size=%d", read_size);
  FOPT ("--write-size=%d", write_size);

  FOPT ("--stat-timeout=%d", stat_timeout);
  FOPT ("--cache-timeout=%d", cache_timeout);
  FOPT ("--init-transmit-timeout=%d", initial_transmit_timeout);
  FOPT ("--max-transmit-timeout=%d", max_transmit_timeout);

  if (err)
    free (argz);

  return err;
}

/* Extract the host and remote filesystem names from SPEC, which should use
   either HOST:FS or FS@HOST notation.  Returns the malloced storage into
   which both REMOTE_FS and HOST point, or 0 if SPEC is invalid.  */
static char *
extract_nfs_args (char *spec, char **remote_fs, char **host)
{
  char *sep;

  spec = strdup (spec);		/* So we can trash it.  */

  sep = index (spec, ':');
  if (sep)
    {
      *sep++ = '\0';
      *host = spec;
      *remote_fs = sep;
      return spec;
    }

  sep = index (spec, '@');
  if (sep)
    {
      *sep++ = '\0';
      *host = sep;
      *remote_fs = spec;
      return spec;
    }

  free (spec);

  return 0;
}

/* Where to find the remote filesystem.  */
static char *remote_fs = 0;
static char *host = 0;

/* For debugging.  */
static volatile int hold = 0;

static error_t
parse_startup_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case OPT_MNT_PORT:
      mount_port_override = 1;
      /* fall through */
    case OPT_MNT_PORT_D:
      mount_port = atoi (arg);
      break;

    case OPT_NFS_PORT:
      nfs_port_override = 1;
      /* fall through */
    case OPT_NFS_PORT_D:
      nfs_port = atoi (arg);
      break;

    case OPT_HOLD: hold = 1; break;

    case ARGP_KEY_ARG:
      if (state->arg_num == 0)
	remote_fs = arg;
      else if (state->arg_num == 1)
	host = arg;
      else
	return EINVAL;
      break;

    case ARGP_KEY_END:
      if (!host && !extract_nfs_args (remote_fs, &remote_fs, &host))
	argp_error (state, "No HOST specified");
      break;

    case ARGP_KEY_NO_ARGS:
      argp_error (state, "No REMOTE_FS specified");

    default:
      return EINVAL;
    }
  return 0;
}


/* NFS client main program */
int
main (int argc, char **argv)
{
  struct argp common_argp = { common_options, parse_common_opt };
  const struct argp *argp_parents[] = { &common_argp, netfs_startup_argp, 0 };
  struct argp argp =
    { startup_options, parse_startup_opt, args_doc, doc, argp_parents };
  mach_port_t bootstrap;
  struct sockaddr_in addr;
  int ret;

  argp_parse (&argp, argc, argv, 0, 0, 0);

  while (hold);
    
  task_get_bootstrap_port (mach_task_self (), &bootstrap);
  netfs_init ();
  
  main_udp_socket = socket (PF_INET, SOCK_DGRAM, 0);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons (IPPORT_RESERVED);
  do
    {
      addr.sin_port = htons (ntohs (addr.sin_port) - 1);
      ret = bind (main_udp_socket, (struct sockaddr *)&addr, 
		  sizeof (struct sockaddr_in));
      if (ret == -1 && errno == EPERM)
	{
	  /* We aren't allowed privileged ports; no matter;
	     let the server deny us later if it wants. */
	  ret = 0;
	  break;
	}
    }
  while ((ret == -1) && (errno == EADDRINUSE));
  if (ret == -1)
    {
      perror ("binding main udp socket");
      exit (1);
    }

  errno = maptime_map (0, 0, &mapped_time);
  if (errno)
    perror ("mapping time");

  cthread_detach (cthread_fork ((cthread_fn_t) timeout_service_thread, 0));
  cthread_detach (cthread_fork ((cthread_fn_t) rpc_receive_thread, 0));
  
  hostname = localhost ();

  netfs_root_node = mount_root (remote_fs, host);

  if (!netfs_root_node)
    exit (1);
  
  netfs_startup (bootstrap, 0);
  
  for (;;)
    netfs_server_loop ();
}

