// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : rsystem.c
// Description : rsystem() service interacting with rsystemd 
//
// License     :
//
//  Copyright (C) 2007-2018 Rachid Koucha <rachid dot koucha at gmail dot com>
//
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to:
// the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor,
// Boston, MA  02110-1301  USA
//
//
// Evolutions  :
//
//     11-Jan-2018 R. Koucha   - Creation
//     07-Mar-2018 R. Koucha   - Bug fix for the status management
//                             - Introduction of an environment variable for
//                               the socket pathname
//                             - Fork() management
//     05-Apr-2018 R. Koucha   - rsystem(): No dynamic allocation for short
//                               command lines
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

#include "rsys_p.h"



// ----------------------------------------------------------------------------
// Name   : rsysd_sd
// Usage  : Socket of RSYSD daemon
// ----------------------------------------------------------------------------
static int rsysd_sd = -1;


// ----------------------------------------------------------------------------
// Name   : RSYSD_BUFFER_SIZE
// Usage  : Size of the buffer
// ----------------------------------------------------------------------------
#define RSYSD_BUFFER_SIZE 4096


// ----------------------------------------------------------------------------
// Name   : rsys_connect
// Usage  : Connect to a RSYS server given its descriptor
// Return : >= 0, socket id
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
static int rsys_connect(void)
{
struct sockaddr_un  server_uaddr;
int                 err_sav;
int                 rc;
int                 sd;
char               *spath;

  // Get the pathname of the socket
  spath = getenv(RSYS_SOCKET_PATH_ENV);
  if (!spath)
  {
    spath = RSYS_SOCKET_PATH;
  }

  // Create a socket
  sd = socket(PF_UNIX, SOCK_STREAM, 0);
  if (sd < 0)
  {
    err_sav = errno;
    RSYS_ERR("socket(%s): '%m' (%d)\n", spath, errno);
    errno = err_sav;
    return -1;
  }

  // Populate the address
  memset(&server_uaddr, 0, sizeof(server_uaddr));
  server_uaddr.sun_family = AF_UNIX;
  rc = snprintf(server_uaddr.sun_path, sizeof(server_uaddr.sun_path), "%s", spath);
  if ((size_t)rc > (sizeof(server_uaddr.sun_path) - 1))
  {
    err_sav = errno;
    RSYS_ERR("Socket's pathname <%s> is too long\n", spath);
    close(sd);
    errno = err_sav;
    return -1;
  }

  // Connect to the server
  rc = connect(sd, (const struct sockaddr *)&server_uaddr, sizeof(server_uaddr));
  if (0 != rc)
  {
    err_sav = errno;
    RSYS_ERR("connect(%s): '%m' (%d)\n", server_uaddr.sun_path, errno);
    errno = err_sav;
    return -1;
  }

  return sd;
} // rsys_connect



//----------------------------------------------------------------------------
// Name        : rsystem
// Description : Remote system() service (submit the command
//               interactively to a running background shell through
//               rsystemd server) 
// Return      : status of the executed command, if OK
//               -1, if error (errno is set)
//----------------------------------------------------------------------------
int rsystem(const char *fmt, ...)
{
int           rc;
va_list       ap;
char         *cmd = (char *)0;
size_t        cmd_len;
int           status = 1;
rsys_msg_t    msg;
char          buffer[RSYSD_BUFFER_SIZE + 1];
unsigned int  i;

  if (rsysd_sd < 0)
  {
    RSYS_ERR("No connection\n");
    status = -1;
    goto end;
  }

  // According to system(3) manual, if the command exists, the status indicates
  // if the shell is available (in the GLIBC source code, an "exit 0" is run to
  // feed the shell)
  if (!fmt)
  {
    // If we are here, this means that the shell exists as it is started by
    // the entry point of this library
    status = 0;
    goto end;
  }

  // Make the command line
  // We first try to format the command in a local buffer to avoid dynamic
  // allocation
  va_start(ap, fmt);
  cmd = buffer;
  rc = vsnprintf(cmd, sizeof(buffer), fmt, ap);
  va_end(ap);

  // If the buffer is too short
  if (rc >= (int)sizeof(buffer))
  {
    // Retry with dynamic allocation
    va_start(ap, fmt);
    rc = vasprintf(&cmd, fmt, ap);
    va_end(ap);

    if (-1 == rc)
    {
      // Error
      RSYS_ERR("vasprintf(%s): '%m' (%d)\n", fmt, errno);
      status = -1;
      goto end;
    }
  }

  if (0 == rc)
  {
    // Same return code as if the command was empty
    status = 0;
    goto end;
  }

  // Length without terminating NUL
  cmd_len = rc;

  // When there are line feeds at the end of the command line,
  // we get multiple prompts on the same line since the echo is deactivated
  // This perturbates the reception of the ending prompt on server side as the regex is a prompt
  // at the beginning of line and nothing behind it: "^ISYS_PDIP> $"
  //
  // So, we remove them here... But there are several other cases of command line configuration which would
  // make the pattern matching fail. The user must be cautious.
  i = cmd_len - 1;
  while (isspace(cmd[i]))
  {
    i --;
  } // End while
  cmd[i + 1] = '\0';
  cmd_len = i + 1;

  //printf("Sending header to the server (%d)...\n", rsysd_sd);

  // Send the command header
  msg.type = RSYS_MSG_CMD;
  msg.length = cmd_len + 1;
  rc = write(rsysd_sd, &msg, sizeof(msg));
  if (rc != (int)sizeof(msg))
  {
    RSYS_ERR("write(%s): '%m' (%d)\n", cmd, errno);
    status = -1;
    goto end;
  }

  // Send the command to the server (with the terminating NUL to
  // facilitate the work on server side)
  rc = write(rsysd_sd, cmd, cmd_len + 1);
  if (rc != (int)(cmd_len + 1))
  {
    RSYS_ERR("write(%s): '%m' (%d)\n", cmd, errno);
    status = -1;
    goto end;
  }

  // Wait for the response
  while (1)
  {
    //printf("Reading header...\n");
    rc = read(rsysd_sd, &msg, sizeof(msg));
    if (rc != sizeof(msg))
    {
      RSYS_ERR("read(header for '%s'), length=%d: '%m' (%d)\n", cmd, rc, errno);
      status = -1;
      goto end;
    }

    //printf("Reading data of msg %d...\n", msg.type);

    switch(msg.type)
    {
      case RSYS_MSG_DISPLAY:
      {
      size_t remain;
      size_t amount;

        remain = msg.length;
        while (remain)
	{
          amount = (remain > RSYSD_BUFFER_SIZE ? RSYSD_BUFFER_SIZE : remain);
          rc = read(rsysd_sd, buffer, amount);
          if (rc < 0)
          {
            RSYS_ERR("read(display for '%s'), length=%d: '%m' (%d)\n", cmd, rc, errno);
            status = -1;
            goto end;
          }

          if (0 == rc)
	  {
            RSYS_ERR("read(display for '%s'): End of connection ?\n", cmd);
            status = -1;
            goto end;
	  }

          remain -= rc;
          buffer[rc] = '\0';
          printf("%s", buffer);
	} // End while remain
      }
      break;

      case RSYS_MSG_EOC :
      {
        //printf("status=%d\n", msg.data.status);
        status = msg.data.status;

        // According to my experimentations, $? is:
        //   . The exit code if the program exited
        //   . (0x80 | signal_number) if the program terminated with a signal 
        //   . 127 (= 0x7F) if the program could not be executed by the shell

        // If it is a signal number
        if (status & 0x80)
        {
          // Keep it as it is since it is a signal
        }
        else // Exit code
        {
          status = status << 8;
        }

        goto end;
      }
      break;

      case RSYS_MSG_BUSY:
      {
        // Not enough context in server
        RSYS_ERR("Server is busy for command '%s'\n", cmd);
        errno = EAGAIN;
        status = -1;
        goto end;
      }
      break;

      case RSYS_MSG_OOM:
      {
        // Memory pb on server side
        RSYS_ERR("Server is out of memory for command '%s'\n", cmd);
        errno = EAGAIN;
        status = -1;
        goto end;
      }
      break;

      default:
      {
        RSYS_ERR("Unexpected message type %d from server for '%s'\n", msg.type, cmd);
        status = -1;
        goto end;
      }
      break;
    } // End switch
  } // End while

end:

  if (cmd && (cmd != buffer))
  {
    free(cmd);
  }

  return status;

} // rsystem





static void rsys_child_fork(void)
{
  // The child process inherited the father's PDIP context
  // but this context references a controlled process which
  // is linked to the father (i.e. father's child)
  // We need to clear the context without killing the
  // controlled process as the father needs it !
  // ==> PDIP library clears all its contexts and deactivate
  //     its signal handler upon fork()

  // Disconnect the service from the server
  if (rsysd_sd >= 0)
  {
    close(rsysd_sd);
    rsysd_sd = -1;
  }

  // If one still needs the RSYS service in a child process,
  // rsys_lib_initialize() must be called explicitely

} // rsys_child_fork


// ----------------------------------------------------------------------------
// Name   : rsys_lib_initialize
// Usage  : Library initialization when needed in a child process
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int rsys_lib_initialize(void)
{
  // Get access to the named socket of rsystemd
  rsysd_sd = rsys_connect();
  if (rsysd_sd < 0)
  {
    return -1;
  }

  return 0;
} // rsys_lib_initialize



static void __attribute__ ((constructor)) rsys_lib_init(void);

// ----------------------------------------------------------------------------
// Name   : rsys_lib_init
// Usage  : Library entry point
// ----------------------------------------------------------------------------
static void rsys_lib_init(void)
{
int rc;

  (void)rsys_lib_initialize();

  rc = pthread_atfork(0, 0, rsys_child_fork);
  if (0 != rc)
  {
    errno = rc;
    RSYS_ERR("pthread_atfork(): '%m' (%d)\n", errno);
  }

} // rsys_lib_init


static void __attribute__ ((destructor)) rsys_lib_exit(void);

// ----------------------------------------------------------------------------
// Name   : rsys_lib_exit
// Usage  : Library exit point
// ----------------------------------------------------------------------------
static void rsys_lib_exit(void)
{
  if (rsysd_sd >= 0)
  {
    close(rsysd_sd);
    rsysd_sd = -1;
  }

} // rsys_lib_exit
