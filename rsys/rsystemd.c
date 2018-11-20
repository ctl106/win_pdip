// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : rsystemd.c
// Description : Daemon managing rsystem() service
//
// License     :
//
//  Copyright (C) 2008-2018 Rachid Koucha <rachid dot koucha at gmail dot com>
//
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//
// Evolutions  :
//
//     11-Jan-2018  R. Koucha   - Creation
//     12-Mar-2018  R. Koucha   - Complete redesign to be more... professional
//     05-Avr-2018  R. Koucha   - No longer ignore SIGINT/SIGQUIT in the shell
//                                as system() ignores those signals in the
//                                father process while running the command (not
//                                in the shell!)
//                                ==> Here we decide not to manage those
//                                signals at all as it is not adapted to
//                                multithreaded applications
//     09-Apr-2018  R. Koucha   - Added daemon mode
//     29-May-2018  R. Koucha   - Inclusion of <string.h> was missing for
//                                memset()
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=



#include "../config.h"
#include <pthread.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdlib.h>
#include <pdip.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <assert.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>

#include "rsys_p.h"


// ----------------------------------------------------------------------------
// Name   : rsysd_dbg_level
// Usage  : Debug level
// ----------------------------------------------------------------------------
static unsigned int rsysd_dbg_level;


// ----------------------------------------------------------------------------
// Name   : rsysd_graceful_lock
// Usage  : Mutual exclusion lock for the graceful thread
// ----------------------------------------------------------------------------
static pthread_mutex_t rsysd_graceful_lock;

static pthread_cond_t rsysd_graceful_cond = PTHREAD_COND_INITIALIZER;

#define RSYSD_GRACEFUL_LOCK()  pthread_mutex_lock(&rsysd_graceful_lock)
#define RSYSD_GRACEFUL_UNLOCK() pthread_mutex_unlock(&rsysd_graceful_lock)


#define RSYSD_GRACEFUL_TIMEOUT 5 // Seconds

static fd_set rsysd_graceful_fdset;
static volatile int rsysd_graceful_fdmax = -1;




// ----------------------------------------------------------------------------
// Name   : rsysd_spath
// Usage  : Socket's pathname
// ----------------------------------------------------------------------------
static char *rsysd_spath;


// ----------------------------------------------------------------------------
// Name   : RSYSD_SHELLS
// Usage  : Environment variable to configure the shells. It is composed of
//          fields separated by a colon. Each field is the affinity of a shell
//          to launch. For examples:
//
//          Empty variable       --> Shell#0 on all the CPUs
//          0:1-3::3,4,6         --> Shell#0 on CPU#0
//                                   Shell#1 on CPU#1 to 3,
//                                   Shell#2 on all CPUs
//                                   Shell#3 on CPU#3, 4 and 6
//          :2                   --> Shell#0 on all CPUs
//                                   Shell#1 on CPU#2
//
// ----------------------------------------------------------------------------
#define RSYSD_SHELLS "RSYSD_SHELLS"



// ----------------------------------------------------------------------------
// Name   : rsysd_version
// Usage  : Version of the software
// ----------------------------------------------------------------------------
static const char *rsysd_version = PDIP_VERSION;



// ----------------------------------------------------------------------------
// Name   : RSYSD_SH_PATH
// Usage  : Shell's pathname
// ----------------------------------------------------------------------------
#define RSYSD_SH_PATH "/bin/sh"


// ----------------------------------------------------------------------------
// Name   : RSYSD_SH_PROMPT
// Usage  : Redefined shell's prompt
// ----------------------------------------------------------------------------
#define RSYSD_SH_PROMPT "RSYSD_PDIP> "


// ----------------------------------------------------------------------------
// Name   : rsysd_longops
// Usage  : Options on the command line
// ----------------------------------------------------------------------------
static struct option rsysd_longopts[] =
{
  { "shells",     required_argument, NULL, 's' },
  { "version",    no_argument,       NULL, 'V' },
  { "debug",      required_argument, NULL, 'd' },
  { "daemon",     no_argument,       NULL, 'D' },
  { "help",       no_argument,       NULL, 'h' },

  // Last entry
  {NULL,          0,                 NULL, 0   }
};






// ----------------------------------------------------------------------------
// Name   : rsysd_shell_t
// Usage  : Context of a shell
// ----------------------------------------------------------------------------
typedef struct
{
  int state;     // State of the context
#define RSYSD_STATE_FREE        0
#define RSYSD_STATE_ALLOCATED   1
#define RSYSD_STATE_WAIT_EOC    2
#define RSYSD_STATE_WAIT_STATUS 3
#define RSYSD_STATE_FREEING     4

#define RSYSD_EVT_LINK          0
#define RSYSD_EVT_CMD           1
#define RSYSD_EVT_DATA          2
#define RSYSD_EVT_CLIENT_DCNX   3

  int fd;        // Connection with the shell

  pdip_t pdip;   // PDIP object/shell to which the client is attached

  // Status of the command
  int status;

  // Display received from the shell
  char   *display;
  size_t  display_sz;
  size_t  display_len;

  // CPU affinity bitmap
  unsigned char  *cpu_affinity;

  // Client to which this shell is attached
  struct rsysd_client *client;

} rsysd_shell_t;


static unsigned int rsysd_nb_shells;
static rsysd_shell_t *rsysd_shells;


// ----------------------------------------------------------------------------
// Name   : rsysd_client_t
// Usage  : Context of a client
// ----------------------------------------------------------------------------
typedef struct rsysd_client
{
  int             state;     // Allocated/deleted

  int             sd;        // Connection with the client

  // Command received from the client
  char           *cmd;
  size_t          cmd_sz;

  rsysd_shell_t *shell;      // Shell to which this client is attached

  struct rsysd_client *prev; // Previous client in the list
  struct rsysd_client *next; // Next client in the list

} rsysd_client_t;


static unsigned int    rsysd_nb_clients;
static rsysd_client_t *rsysd_clients;



// ----------------------------------------------------------------------------
// Name   : rsysd_fdset
// Usage  : Set of file descriptors for the main's engine
// ----------------------------------------------------------------------------
static fd_set rsysd_fdset;


// ----------------------------------------------------------------------------
// Name   : rsysd_fd_max
// Usage  : Greatest file descriptor in the set of the main's engine
// ----------------------------------------------------------------------------
static int rsysd_fd_max;


// ----------------------------------------------------------------------------
// Name   : rsysd_loop
// Usage  : Looping flag
// ----------------------------------------------------------------------------
static int rsysd_loop = 1;


//---------------------------------------------------------------------------
// Name : rsysd_add_set
// Usage: Add a file descriptor in a set
//----------------------------------------------------------------------------
static void rsysd_add_set(
			  int            fd,
                          fd_set        *fdset,
                          volatile int  *fd_max
                         )
{

  FD_SET(fd, fdset);

  if (fd > *fd_max)
  {
    *fd_max = fd;
  }
} // rsysd_add_set


//---------------------------------------------------------------------------
// Name : rsysd_remove_set
// Usage: Remove a file descriptor from a bitmap set
//        The maximum file descriptor id is updated
//----------------------------------------------------------------------------
static void rsysd_remove_set(
                             int           fd,
                             fd_set       *fdset,
			     volatile int *fd_max
                            )
{
  FD_CLR(fd, fdset);

  // Update fd_max if necessary
  if (*fd_max == fd)
  {
  int i;

    for (i = (fd - 1); i >= 0; i --)
    {
      if (FD_ISSET(i, fdset))
      {
        *fd_max = i;
        break;
      }
    } // End for
  }
} // rsysd_remove_set




// ----------------------------------------------------------------------------
// Name   : rsysd_graceful_thd
// Usage  : Entry point of the graceful thread which is in charge to
//          close gracefully the sockets with clients when the contexts are
//          full
// ----------------------------------------------------------------------------
static void *rsysd_graceful_thd(void *p)
{
int            rc;
int            nfds;
fd_set         fdset, orig_fdset;
int            i;
struct timeval to;

  (void)p;

  while(1)
  {
    // Before blocking, make sure that there are no file descriptors
    // to close
    RSYSD_GRACEFUL_LOCK();
    if (rsysd_graceful_fdmax < 0)
    {
      // Wait for a file descriptor to close
      // The following call releases the lock
      rc = pthread_cond_wait(&rsysd_graceful_cond, &rsysd_graceful_lock);
      assert(0 == rc);
    }

    // The lock is taken here

    // We must have opened file descriptors
    if (rsysd_graceful_fdmax < 0)
    {
      // There is an incohrency !
      RSYSD_ERR("Bug as rsysd_graceful_fdmax=%d\n", rsysd_graceful_fdmax);
      assert(0);
    }

    nfds = rsysd_graceful_fdmax + 1;
    fdset = rsysd_graceful_fdset;

    // The current set of file descriptors to close after the timeout
    orig_fdset = rsysd_graceful_fdset;

    // Release the lock
    RSYSD_GRACEFUL_UNLOCK();

    // Wait some seconds or the closing of one of the file
    // descriptors by the client before waking up...
    to.tv_sec = RSYSD_GRACEFUL_TIMEOUT;
    to.tv_usec = 0;
    rc = select(nfds, &fdset, NULL, NULL, &to);

    switch(rc)
    {
      case -1:
      {
        // This may be a timeout or anything else
        // ==> Close all the sockets
        for (i = 0; i < nfds; i ++)
	{
          if (FD_ISSET(i, &orig_fdset))
	  {
            RSYSD_GRACEFUL_LOCK();
	    RSYSD_ERR("Error or timeout (%m - %d), closing socket %u\n", errno, i);
            shutdown(i, SHUT_RDWR);
            close(i);

            rsysd_remove_set(i, &rsysd_graceful_fdset, &rsysd_graceful_fdmax);

            RSYSD_GRACEFUL_UNLOCK();
	  }
	} // End for
      }
      break;

      default :
      {
        // At least one of the file descriptors has been closed
        for (i = 0; (i < nfds) && rc; i ++)
	{
          if (FD_ISSET(i, &fdset))
	  {
            RSYSD_GRACEFUL_LOCK();

            shutdown(i, SHUT_RDWR);
            
	    close(i);
            rsysd_remove_set(i, &rsysd_graceful_fdset, &rsysd_graceful_fdmax);

            RSYSD_GRACEFUL_UNLOCK();

            // One less file descriptor
            rc --;
	  }
	} // End for
      }
      break;
    }  // End switch
  } // End while

  return NULL;
} // rsysd_graceful_thd




// ----------------------------------------------------------------------------
// Name   : rsysd_graceful_close
// Usage  : Close a socket gracefully
// Return : None
// ----------------------------------------------------------------------------
static void rsysd_graceful_close(int sd)
{
  // Let some time to the client to get the answer thanks to the
  // graceful thread
  RSYSD_GRACEFUL_LOCK();

  rsysd_add_set(sd, &rsysd_graceful_fdset, &rsysd_graceful_fdmax);

  // Wake up the graceful thread
  (void)pthread_cond_signal(&rsysd_graceful_cond);

  RSYSD_GRACEFUL_UNLOCK();
} // bci_graceful_close




// ----------------------------------------------------------------------------
// Name   : rsysd_get_spath
// Usage  : Make the server's socket pathname
// Return : None
// ----------------------------------------------------------------------------
static void rsysd_get_spath(void)
{
  // Get the pathname of the socket
  rsysd_spath = getenv(RSYS_SOCKET_PATH_ENV);
  if (!rsysd_spath)
  {
    rsysd_spath = RSYS_SOCKET_PATH;
  }
  else
  {
    RSYSD_DBG(1, "Using environment for socket pathname: %s\n", rsysd_spath);
  }
} // rsysd_get_spath



// ----------------------------------------------------------------------------
// Name   : rsysd_open_socket
// Usage  : Create the server socket
// Return : Socket descriptor if OK
//          -1, if error
// ----------------------------------------------------------------------------
static int rsysd_open_socket(void)
{
int                 sockVal;
struct sockaddr_un  server_uaddr;
int                 sd;
int                 rc;
int                 err_sav;

  // Create a Linux socket
  sd = socket(PF_UNIX, SOCK_STREAM, 0);
  if (sd < 0)
  {
    err_sav = errno;
    RSYSD_ERR("socket(%s): '%m' (%d)\n", rsysd_spath, errno);
    errno = err_sav;
    return -1;
  }

  // Set some options on the socket
  sockVal = 1;
  if (0 != setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,
		      (char *) &sockVal, sizeof (sockVal)))
  {
    err_sav = errno;
    RSYSD_ERR("setsockopt(SO_REUSEADDR, %s): '%m' (%d)\n", rsysd_spath, errno);
    close(sd);
    errno = err_sav;
    return -1;
  }

  // Bind the socket
  memset(&server_uaddr, 0, sizeof(server_uaddr));
  server_uaddr.sun_family = AF_UNIX;
  rc = snprintf(server_uaddr.sun_path, sizeof(server_uaddr.sun_path), "%s", rsysd_spath);
  if ((size_t)rc > (sizeof(server_uaddr.sun_path) - 1))
  {
    err_sav = errno;
    RSYSD_ERR("Socket's pathname <%s> is too long\n", rsysd_spath);
    close(sd);
    errno = err_sav;
    return -1;
  }

  // Remove an existing socket file if any
  (void)unlink(server_uaddr.sun_path);

  rc = bind(sd, (struct sockaddr *)&server_uaddr, sizeof(server_uaddr));
  if (rc != 0)
  {
    err_sav = errno;
    RSYSD_ERR("bind(%s): '%m' (%d)\n", server_uaddr.sun_path, errno);
    close(sd);
    (void)unlink(server_uaddr.sun_path);
    errno = err_sav;
    return -1;
  }

  // Update access rights on the socket file to make sure
  // that any user can connect to it
  rc = chmod(server_uaddr.sun_path, 0777);

  // Set the input connection queue length
  if (listen(sd, 5) == -1)
  {
    err_sav = errno;
    RSYSD_ERR("listen(%s): '%m' (%d)\n", server_uaddr.sun_path, errno);
    close(sd);
    (void)unlink(server_uaddr.sun_path);
    errno = err_sav;
    return -1;
  }

  // Return the socket
  return sd;
} // rsysd_open_socket



// ----------------------------------------------------------------------------
// Name   : rsysd_get_affinity
// Usage  : Analyse an affinity string
// Syntax :
//
//  . List of comma separated fields terminated by NUL or ':'
//  . field = CPU number
//          = CPU number-CPU number = Interval of consecutive CPU numbers
//  . A CPU number is from 0 to number of active CPUs - 1
//  . If one of the numbers is bigger than the number of active CPUs, it is
//    translated into the biggest CPU number
//
// Return : 0, if OK
//          -1, if error
// ----------------------------------------------------------------------------
int rsysd_get_affinity(
		       const char    *affinity,
                       unsigned char *cpu_bitmap,
                       unsigned int  *offset
	 	      )
{
const char   *p;
unsigned int  start, end, i;

 (void)pdip_cpu_zero(cpu_bitmap);

  *offset = 0;
  p = affinity;

// Beginning

  switch(*p)
  {
    case '\0':
    case ':':
    {
      (void)pdip_cpu_all(cpu_bitmap);
      *offset = p - affinity;
      return 0;
    }
    break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    {
      start = *p - '0';
      p ++;
      while (isdigit(*p))
      {
        start *= 10;
        start += (*p - '0');
        p ++;
      } // End while
      if (start >= pdip_cpu_nb())
      {
        start = pdip_cpu_nb() - 1;
      }
      goto state_2;
    }
    break;

    case '-':
    {
      start = 0;
      p ++;
      goto state_3;
    }
    break;

    case ',':
    {
      (void)pdip_cpu_all(cpu_bitmap);
      p ++;
      goto state_1;
    }
    break;

    default:
    {
      *offset = p - affinity;
      return -1;
    }
  } // End switch

state_1: // New field (after ",")

  switch(*p)
  {
    case '\0':
    case ':':
    {
      (void)pdip_cpu_all(cpu_bitmap);
      *offset = p - affinity;
      return 0;
    }
    break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    {
      start = *p - '0';
      p ++;
      while (isdigit(*p))
      {
        start *= 10;
        start += (*p - '0');
        p ++;
      } // End while
      if (start >= pdip_cpu_nb())
      {
        start = pdip_cpu_nb() - 1;
      }
      goto state_2;
    }
    break;

    case '-':
    {
      start = 0;
      p ++;
      goto state_3;
    }
    break;

    case ',':
    {
      (void)pdip_cpu_all(cpu_bitmap);
      p ++;
      goto state_1;
    }
    break;

    default:
    {
      *offset = p - affinity;
      return -1;
    }
  } // End switch

state_2: // Interval "-number"

  switch(*p)
  {
    case '\0':
    case ':' :
    {
      (void)pdip_cpu_set(cpu_bitmap, start);
      *offset = p - affinity;
      return 0;
    }
    break;

    case ',':
    {
      (void)pdip_cpu_set(cpu_bitmap, start);
      p ++;
      goto state_1;
    }
    break;

    case '-':
    {
      p ++;
      goto state_3;
    }
    break;

    default:
    {
      *offset = p - affinity;
      return -1;
    }
  } // End switch


state_3: // Interval "ending number"

  switch(*p)
  {
    case '\0':
    case ':':
    {
      end = pdip_cpu_nb() - 1;
      for (i = start; i <= end; i ++)
      {
        (void)pdip_cpu_set(cpu_bitmap, i);
      }
      *offset = p - affinity;
      return 0;
    }
    break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    {
      end = *p - '0';
      p ++;
      while (isdigit(*p))
      {
        end *= 10;
        end += (*p - '0');
        p ++;
      } // End while
      if (end >= pdip_cpu_nb())
      {
        end = pdip_cpu_nb() - 1;
      }
      if (end < start)
      {
        *offset = p - affinity;
        return -1;
      }
      for (i = start; i <= end; i ++)
      {
        (void)pdip_cpu_set(cpu_bitmap, i);
      }

      if (',' == *p)
      {
        goto state_1;
      }

      *offset = p - affinity;

      if (':' == *p || '\0' == *p)
      {
        return 0;
      }
      else
      {
        return -1;
      }      
    }
    break;

    default:
    {
      *offset = p - affinity;
      return -1;
    }
  } // End switch

} // rsysd_get_affinity


// ----------------------------------------------------------------------------
// Name   : rsysd_display_affinity
// Usage  : Display the CPU affinity of a given shell
// Return : 0, if OK
//          -1, if error
// ----------------------------------------------------------------------------
void rsysd_display_affinity(rsysd_shell_t *shell)
{
unsigned int i;
unsigned int nb = pdip_cpu_nb();

  for (i = 0; i < nb; i ++)
  {
    if (pdip_cpu_isset(shell->cpu_affinity, i))
    {
      RSYSD_DBG(1, "\tCPU#%u\n", i);
    }
  } // End for
} // rsysd_display_affinity


// ----------------------------------------------------------------------------
// Name   : rsysd_create_shells
// Usage  : Create the shells
// Return : 0, if OK
//          -1, if error
// ----------------------------------------------------------------------------
static int rsysd_create_shells(const char *shells)
{
char           *av[2];
pdip_cfg_t      cfg;
int             rc;
struct timeval  to;
unsigned int    i;
rsysd_shell_t  *shell;
const char     *p, *p1;
unsigned int    offset;
pdip_t          pdip;

  // The environment variable is prioritary (if launched
  // with sudo, the -E option must be passed to preserve environment)
  p = getenv(RSYSD_SHELLS);
  if (!p)
  {
    p = shells;
  }
  else
  {
    RSYSD_DBG(1, "Using environment for shells: %s\n", p);
  }

  // Count the number of shells
  p1 = p;
  if (p1)
  {
    rsysd_nb_shells = 0;
    while (*p1)
    {
      if (':' == *p1)
      {
        rsysd_nb_shells ++;
      }

      p1 ++;
    } // End while

    rsysd_nb_shells ++;
  }
  else
  {
    rsysd_nb_shells = 1;
  }

  RSYSD_DBG(1, "Number of shells: %u\n", rsysd_nb_shells);

  // Allocate the table of contexts
  rsysd_shells = (rsysd_shell_t *)malloc(sizeof(rsysd_shell_t) * rsysd_nb_shells);
  if (!rsysd_shells)
  {
    RSYSD_ERR("malloc(%zu): '%m' (%d)\n", sizeof(rsysd_shell_t) * rsysd_nb_shells, errno);
    return -1;
  }

  // Get the CPU affinity of the shells to launch
  p1 = p;
  if (p1)
  {
    i = 0;
    while (1)
    {
      shell = &(rsysd_shells[i]);

      // Allocate the CPU affinity bitmap
      shell->cpu_affinity = pdip_cpu_alloc();
      if (!(shell->cpu_affinity))
      {
        RSYSD_ERR("pdip_cpu_alloc() for shell#%u: '%m' (%d)\n", i, errno);
      }

      rc = rsysd_get_affinity(p1, shell->cpu_affinity, &offset);
      if (rc != 0)
      {
        RSYSD_ERR("Syntax error in affinity '%s' at offset %u\n", p1, offset);
        errno = EINVAL;
        return -1;
      }

      p1 += offset;
      if (':' == *p1)
      {
        // Next field
        p1 ++;
        i ++;
      }
      else
      {
        // No more fields
        break;
      }
    } // End while

    assert(i == (rsysd_nb_shells - 1));
  }
  else
  {
    shell = &(rsysd_shells[0]);
    shell->cpu_affinity = pdip_cpu_alloc();
    (void)pdip_cpu_all(shell->cpu_affinity);
  } // End if shells

  // Configure the PDIP library
  rc = pdip_configure(1, 0);
  if (0 != rc)
  {
    RSYSD_ERR("pdip_configure(): '%m' (%d)\n", errno);
    return -1;
  }

  // Set the prompt of the shell
  rc = setenv("PS1", RSYSD_SH_PROMPT, 1);
  if (0 != rc)
  {
    RSYSD_ERR("setenv(PS1): '%m' (%d)\n", errno);
    return -1;
  }

  // Configure the PDIP object (By default, the process attached to it
  // can run on any processor)
  (void)pdip_cfg_init(&cfg);
  cfg.dbg_output = stderr;
  cfg.err_output = stderr;
  cfg.debug_level = 0;
  // The prompt of the shell is displayed onto stderr
  // Hence, we redirect stderr into the PDIP PTY to be able
  // to synchronize on it
  // Use PDIP_FLAG_RECV_ON_THE_FLOW to increase parallelism
  // when command are too verbose
  cfg.flags = PDIP_FLAG_ERR_REDIRECT | PDIP_FLAG_RECV_ON_THE_FLOW;

  // Make the reception buffer resizing increment quite large
  cfg.buf_resize_increment = 4 * 1024;

  // Initialize the contexts of the clients
  for (i = 0; i < rsysd_nb_shells; i ++)
  {
    shell = &(rsysd_shells[i]);

    // CPU affinity
    cfg.cpu = shell->cpu_affinity;

    // Create the PDIP object
    pdip = pdip_new(&cfg);
    if (!pdip)
    {
      RSYSD_ERR("pdip_new(): '%m' (%d)\n", errno);
      return -1;
    }

    //pdip_set_debug_level(pdip, 10);

    RSYSD_DBG(1, "Launching client#%u with affinity:\n", i);
    rsysd_display_affinity(shell);

    // Attach a shell to the PDIP object
    av[0] = RSYSD_SH_PATH;
    av[1] = (char *)0;
    rc = pdip_exec(pdip, 1, av);
    if (rc < 0)
    {
      RSYSD_ERR("pdip_exec(): '%m' (%d)\n", errno);
      return -1;
    }

    // Populate the context now as we are connected to a shell
    // (pdip_fd() is valid and we need to init the buffers for
    // the following services)
    shell->state       = RSYSD_STATE_FREE;
    shell->fd          = pdip_fd(pdip);
    shell->pdip        = pdip;
    shell->status      = -1;
    shell->display     = (char *)0;
    shell->display_sz  = 0;
    shell->display_len = 0;

    // Wait for the 1st prompt
    do
    {
      to.tv_sec = 5;
      to.tv_usec = 0;
      rc = pdip_recv(pdip, "^" RSYSD_SH_PROMPT "$",
                     &(shell->display), &(shell->display_sz), &(shell->display_len), &to);
      if ((rc != PDIP_RECV_DATA) && (rc != PDIP_RECV_FOUND))
      {
        RSYSD_ERR("pdip_recv(): Didn't received the prompt (rc = %d)?!?\n", rc);
        return -1;
      }
    } while (PDIP_RECV_FOUND != rc);

    // Disable the echo of the characters to make the transactions faster
    rc = pdip_send(pdip, "%s\n", "stty -echo");
    if (rc < 0)
    {
      RSYSD_ERR("pdip_send(): '%m' (%d)\n", errno);
      return -1;
    }

    // Wait for the prompt
    do
    {
      to.tv_sec = 5;
      to.tv_usec = 0;
      rc = pdip_recv(pdip, "^" RSYSD_SH_PROMPT "$",
                     &(shell->display), &(shell->display_sz), &(shell->display_len), &to);
      if ((rc != PDIP_RECV_DATA) && (rc != PDIP_RECV_FOUND))
      {
        RSYSD_ERR("pdip_recv(): Didn't received the prompt (rc = %d)?!?\n", rc);
        return -1;
      }
    } while (PDIP_RECV_FOUND != rc);

  } // End for all the contexts

  return 0;
} // rsysd_create_shells



// ----------------------------------------------------------------------------
// Name   : rsysd_delete_shells
// Usage  : Delete the running shells
// Return : None
// ----------------------------------------------------------------------------
static void rsysd_delete_shells(void)
{
unsigned int   i;
rsysd_shell_t *shell;

  for (i = 0; i < rsysd_nb_shells; i ++)
  {
    shell = &(rsysd_shells[i]);
    (void)pdip_delete(shell->pdip, 0);
  } // End for

} // rsysd_delete_shells



// ----------------------------------------------------------------------------
// Name   : rsysd_send_cmd
// Usage  : Submit a command line to a shell
// Return : 0, if OK
//          -1, if error
// ----------------------------------------------------------------------------
static int rsysd_send_cmd(
                          rsysd_client_t *client,
                          const char     *cmd
                         )
{
int rc;

  assert(client->shell);

  // Send the command
  rc = pdip_send(client->shell->pdip, "%s\n", cmd);
  if (rc < 0)
  {
    RSYSD_ERR("pdip_send(%s): '%m' (%d)\n", cmd, errno);
    return -1;
  }

  return 0;

} // rsysd_send_cmd


#define rsysd_run_cmd(client) rsysd_send_cmd(client, client->cmd)
#define rsysd_request_status(client) rsysd_send_cmd(client, "echo $?")

// ----------------------------------------------------------------------------
// Name   : rsysd_read_shell_display
// Usage  : Read answer a shell
// Return : PDIP_RECV_DATA, if data received
//          PDIP_RECV_FOUND, if end of data
//          -1, if error
// ----------------------------------------------------------------------------
static int rsysd_read_shell_display(rsysd_shell_t *shell)
{
int   rc;
char *p;

  // Wait for the prompt (no timeout as we now that data are available with the select() call)
  rc = pdip_recv(shell->pdip, "^" RSYSD_SH_PROMPT "$",
                 &(shell->display), &(shell->display_sz), &(shell->display_len), 0);
  switch(rc)
  {
    case PDIP_RECV_FOUND:
    {
      // Point at the end of "display of program\nprompt"
      p = shell->display + shell->display_len; // End of display

      if ((sizeof(RSYSD_SH_PROMPT) - 1) > shell->display_len)
      {
        RSYSD_ERR("Incoherent display from the shell: are we desynchronized?!? display_len=%zu, display_sz=%zu, display='%s', cmd='%s'\n",
                 shell->display_len, shell->display_sz, shell->display, shell->client->cmd);
        return -1;
      }
 
      // Point before the "prompt"
      p -= (sizeof(RSYSD_SH_PROMPT) - 1);

      // If there are data before the prompt
      if (p > shell->display)
      {
        if (*(p - 1) != '\n')
        {
          RSYSD_ERR("Incoherent display from the shell: are we desynchronized?!? display_len=%zu, display_sz=%zu, display='%s', cmd='%s', p='%s'\n",
                   shell->display_len, shell->display_sz, shell->display, shell->client->cmd, p);
          return -1;
        }

        // Put NUL right after the last '\n' before the prompt
        *p = '\0';
        rc = rsys_send_msg_data(shell->client->sd, RSYS_MSG_DISPLAY, p - shell->display + 1, shell->display);
        if (rc != 0)
        {
          RSYSD_ERR("rsys_send_msg_data(%zu bytes): '%m' (%d)\n", p - shell->display + 1, errno);
          return -1;
        }
      }

      return PDIP_RECV_FOUND;
    }
    break;

    case PDIP_RECV_DATA:
    {
      // Send the output of the program (if any) to the client (NUL terminated)
      if (shell->display_len)
      {
        rc = rsys_send_msg_data(shell->client->sd, RSYS_MSG_DISPLAY, shell->display_len + 1, shell->display);
        if (rc != 0)
        {
          RSYSD_ERR("rsys_send_msg_data(%zu bytes): '%m' (%d)\n", shell->display_len, errno);
          return -1;
        }
      }

      return PDIP_RECV_DATA;
    }
    break;

    default:
    {
      RSYSD_ERR("pdip_recv(%s): Didn't received the shell prompt, rc = %d, errno='%m' (%d) ?!?\n", shell->client->cmd, rc, errno);
      return -1;
    }
    break;
  } // End switch

} // rsysd_read_shell_display





// ----------------------------------------------------------------------------
// Name   : rsysd_read_shell_status
// Usage  : Read answer a shell
// Return : PDIP_RECV_DATA, if data received
//          PDIP_RECV_FOUND, if end of data
//          -1, if error
// ----------------------------------------------------------------------------
static int rsysd_read_shell_status(rsysd_shell_t *shell)
{
int   rc;
char *p;

  // Wait for the prompt (no timeout as we now that data are available with the select() call)
  rc = pdip_recv(shell->pdip, "^" RSYSD_SH_PROMPT "$",
                 &(shell->display), &(shell->display_sz), &(shell->display_len), 0);
  switch(rc)
  {
    case PDIP_RECV_FOUND:
    {
      if (shell->display_len > sizeof(RSYSD_SH_PROMPT))
      {
        // The status is coming with the prompt : "status\nprompt"
        p = shell->display + shell->display_len;

        // Point before the "\nprompt"
        p -= sizeof(RSYSD_SH_PROMPT);
        if (*p != '\n')
        {
          RSYSD_ERR("Incoherent display from the shell: are we desynchronized?!? display_len=%zu, display_sz=%zu, display='%s', cmd='%s', p='%s'\n",
                   shell->display_len, shell->display_sz, shell->display, shell->client->cmd, p);
          return -1;
        }

        // NUL terminate the status
        *p = '\0';

        // Translate the status into an integer
        shell->status = atoi(shell->display);
      }
      else
      {
        // The status is already received
        assert(shell->display_len == (sizeof(RSYSD_SH_PROMPT) - 1));
      }

      return PDIP_RECV_FOUND;
    }
    break;

    case PDIP_RECV_DATA:
    {
      // Sometimes the status may come in a packet independant from the prompt

      p = shell->display + shell->display_len - 1; // End of line
      if (*p != '\n')
      {
        RSYSD_ERR("Incoherent display from the shell: are we desynchronized?!? display_len=%zu, display_sz=%zu, display='%s', cmd='%s', p='%s'\n",
                  shell->display_len, shell->display_sz, shell->display, shell->client->cmd, p);
        return -1;
      }

      // NUL terminate the status
      *p = '\0';

      // Translate the status into an integer
      shell->status = atoi(shell->display);

      return PDIP_RECV_DATA;
    }
    break;

    default:
    {
      RSYSD_ERR("pdip_recv(%s): Didn't received the shell prompt, rc = %d, errno='%m' (%d) ?!?\n", shell->client->cmd, rc, errno);
      return -1;
    }
    break;
  } // End switch

} // rsysd_read_shell_status



// ----------------------------------------------------------------------------
// Name   : rsysd_flush_shell_display
// Usage  : Read data from a shell until a prompt appears
// Return : PDIP_RECV_DATA, if data received
//          PDIP_RECV_FOUND, if end of data
//          -1, if error
// ----------------------------------------------------------------------------
static int rsysd_flush_shell_display(rsysd_shell_t *shell)
{
int   rc;

  // Wait for the prompt (no timeout as we now that data are available with the select() call)
  rc = pdip_recv(shell->pdip, "^" RSYSD_SH_PROMPT "$",
                 &(shell->display), &(shell->display_sz), &(shell->display_len), 0);
  switch(rc)
  {
    case PDIP_RECV_FOUND:
    {
      return PDIP_RECV_FOUND;
    }
    break;

    case PDIP_RECV_DATA:
    {
      return PDIP_RECV_DATA;
    }
    break;

    default:
    {
      RSYSD_ERR("pdip_recv(): Didn't received the shell prompt, rc = %d, errno='%m' (%d) ?!?\n", rc, errno);
      return -1;
    }
    break;
  } // End switch

} // rsysd_flush_shell_display



// ----------------------------------------------------------------------------
// Name   : rsysd_delete_client
// Usage  : Free a client's context
// Return : Context if OK
//          0, if error
// ----------------------------------------------------------------------------
static void rsysd_delete_client(
                                rsysd_client_t *client
                               )
{
  assert(RSYSD_STATE_ALLOCATED == client->state);
  client->state = RSYSD_STATE_FREE;
  client->shell = (rsysd_shell_t *)0;
} // rsysd_delete_client


// ----------------------------------------------------------------------------
// Name   : rsysd_free_client
// Usage  : Free a client's context
// Return : Context if OK
//          0, if error
// ----------------------------------------------------------------------------
static void rsysd_free_client(
                                rsysd_client_t *client
                               )
{
  assert(RSYSD_STATE_FREE == client->state);

  // Unlink the context
  if (client->prev)
  {
    client->prev->next = client->next;
  }
  else
  {
    assert(rsysd_clients == client);
    rsysd_clients = client->next;
  }

  if (client->next)
  {
    client->next->prev = client->prev;
  }

  // Remove the socket from the global fdset
  rsysd_remove_set(client->sd, &rsysd_fdset, &rsysd_fd_max);

  // Gracefully close the socket
  rsysd_graceful_close(client->sd);

  // For debug purposes, before freeing, we unset some fields
  client->sd = -1;

  
  assert((rsysd_shell_t *)0 == client->shell);

  // Free the memory
  if (client->cmd)
  {
    free(client->cmd);
    client->cmd = (char *)0;

    assert(client->cmd_sz);
    client->cmd_sz = 0;
  }
  free(client);

  assert(rsysd_nb_clients > 0);
  rsysd_nb_clients --;

} // rsysd_free_client



// ----------------------------------------------------------------------------
// Name   : rsysd_unlink_client
// Usage  : Unlink a client from a shell
// Return : None
// ----------------------------------------------------------------------------
static void rsysd_unlink_client(
                                rsysd_shell_t *shell
                               )
{
  assert(shell->client);
  assert(shell->client->shell);
  shell->client->shell = (rsysd_shell_t *)0;
  shell->client = (rsysd_client_t *)0;
} // rsysd_unlink_client


// ----------------------------------------------------------------------------
// Name   : rsysd_shell_fsm
// Usage  : Engine of the shell FSM
// Return : new state of the FSM, if OK
//          -1, if error
// ----------------------------------------------------------------------------
static int rsysd_shell_fsm(
                           rsysd_shell_t *shell,
                           int            evt,
                           void          *data
                          )
{
rsysd_client_t *client;
int             rc;

  RSYSD_DBG(4, "IN STATE: %d / EVT: %d\n", shell->state, evt);

  switch(shell->state)
  {
    case RSYSD_STATE_FREE :
    {
      switch(evt)
      {
        case RSYSD_EVT_LINK : // Linking to a client
	{
          client = (rsysd_client_t *)data;

	  RSYSD_DBG(5, "RSYSTEMD(%d): LINKING %d\n", getpid(), client->sd);

          // Link the shell and the client
          shell->client = client;
          client->shell = shell;

          shell->state = RSYSD_STATE_ALLOCATED;
	}
        break;

        default:
	{
          goto unexpected_evt;
	}
        break;
      } // End switch(evt)      
    }
    break;

    case RSYSD_STATE_ALLOCATED :
    {
      switch(evt)
      {
        case RSYSD_EVT_CMD : // Command from the client
	{
          client = shell->client;
          assert(client);

          // Submit the command to the shell
          rc = rsysd_run_cmd(client);
          if (0 == rc)
	  {
            shell->state = RSYSD_STATE_WAIT_EOC;
	  }
          else
	  {
            // Send a dummy error status to the client
            (void)rsys_send_eoc(shell->client->sd, -1);
            RSYSD_ERR("Sent a dummy error status (-1) to the client (%s)\n", shell->client->cmd);

  	    // Unlink the client
	    rsysd_unlink_client(shell);            
 
            shell->state = RSYSD_STATE_FREE;
	  }
	}
        break;

        case RSYSD_EVT_CLIENT_DCNX : // Client disconnection
	{
          // Free the client's context
          rsysd_delete_client(shell->client);
          shell->client = (rsysd_client_t *)0;

          // Go to FREE state as the shell has not been sollicited: no pending data to read
          shell->state = RSYSD_STATE_FREE;
	}
        break;

        default:
	{
          goto unexpected_evt;
	}
        break;
      } // End switch(evt)
    }
    break;

    case RSYSD_STATE_WAIT_EOC :
    {
      switch(evt)
      {
        case RSYSD_EVT_DATA : // Data from the shell
	{
          // We expect to get the prompt from the shell but
          // we may receive some displays before...
          rc = rsysd_read_shell_display(shell);
          if (PDIP_RECV_FOUND == rc)
	  {
            // Prompt received

            // Send status request
            rc = rsysd_request_status(shell->client);
            if (0 == rc)
	    {
              shell->state = RSYSD_STATE_WAIT_STATUS;
	    }
            else
	    {
              // Should we restart the shell ?

              // Send a dummy error status to the client
              (void)rsys_send_eoc(shell->client->sd, -1);
              RSYSD_ERR("Sent a dummy error status (-1) to the client (%s)\n", shell->client->cmd);

  	      // Unlink the client
	      rsysd_unlink_client(shell);
 
              shell->state = RSYSD_STATE_FREE;
	    }
	  }
          else if (PDIP_RECV_DATA == rc)
	  {
            // We stay in current state to get the prompt
	  }
          else // Error
	  {
            // Should we restart the shell ?

            // Send a dummy error status to the client
            (void)rsys_send_eoc(shell->client->sd, -1);
            RSYSD_ERR("Sent a dummy error status (-1) to the client (%s)\n", shell->client->cmd);

	    // Unlink the client
	    rsysd_unlink_client(shell);

            shell->state = RSYSD_STATE_FREE;
	  }
	}
        break;

        case RSYSD_EVT_CLIENT_DCNX : // Client disconnection
	{
          // Free the client's context
          rsysd_delete_client(shell->client);
          shell->client = (rsysd_client_t *)0;

          // The status is expected from the shell
          shell->state = RSYSD_STATE_FREEING;
	}
        break;

        default:
	{
          goto unexpected_evt;
	}
        break;
      } // End switch(evt)
    }
    break;

    case RSYSD_STATE_WAIT_STATUS :
    {
      switch(evt)
      {
        case RSYSD_EVT_DATA : // Data from the shell
	{
          // We expect to get the prompt from the shell but
          // we may receive some displays before...
          rc = rsysd_read_shell_status(shell);
          if (PDIP_RECV_FOUND == rc)
	  {
            // Prompt received

            // The preceding call has sent the status to the client

            // Send the end of command to the client
            rc = rsys_send_eoc(shell->client->sd, shell->status);
            if (rc != 0)
            {
              RSYSD_ERR("rsys_send_eoc(%s, %d): '%m (%d)\n", shell->client->cmd, shell->status, errno);

              // Anyway we disconnect the client and go into the FREE state
            }

            // Unlink the client
            rsysd_unlink_client(shell);

            shell->state = RSYSD_STATE_FREE;
	  }
          else if (PDIP_RECV_DATA == rc)
	  {
            // We stay in current state to get the prompt
	  }
          else // Error
	  {
            // Should we restart the shell ?

            // Send a dummy error status to the client
            (void)rsys_send_eoc(shell->client->sd, -1);
            RSYSD_ERR("Sent a dummy error status (-1) to the client (%s)\n", shell->client->cmd);

	    // Unlink the client
            rsysd_unlink_client(shell);

            shell->state = RSYSD_STATE_FREE;
	  }
	}
        break;

        case RSYSD_EVT_CLIENT_DCNX : // Client disconnection
	{
          // Free the client's context
          rsysd_delete_client(shell->client);
          shell->client = (rsysd_client_t *)0;

          shell->state = RSYSD_STATE_FREEING;
	}
        break;

        default:
	{
          goto unexpected_evt;
	}
        break;
      } // End switch(evt)
    }
    break;

    case RSYSD_STATE_FREEING :
    {
      switch(evt)
      {
        case RSYSD_EVT_DATA : // Data from the shell
	{
          // Look for the prompt which marks the end of the running
          // command (shell command or status request)
          rc = rsysd_flush_shell_display(shell);
          if (PDIP_RECV_FOUND == rc)
	  {
            shell->state = RSYSD_STATE_FREE;
	  }
          else if (PDIP_RECV_DATA == rc)
	  {
            // We stay in current state to get the prompt
	  }
          else // Error
	  {
	    // Should we restart the shell here ?
            shell->state = RSYSD_STATE_FREE;
	  }
	}
        break;

        case RSYSD_EVT_CLIENT_DCNX : // Client disconnection
	{
          // This event is possible in the same select() loop (received data
          // from shell then we receive DCNX from client in the same select()
          // loop)

          // Nothing to do
	}
        break;

        default:
	{
          goto unexpected_evt;
	}
        break;
      } // End switch(evt)      
    }
    break;

    default:
    {
      goto unexpected_evt;
    }
    break;
  } // End switch(state)

  RSYSD_DBG(4, "OUT STATE: %d\n", shell->state);

  return shell->state;

unexpected_evt:

  RSYSD_ERR("Unexpected event %d in state %d\n", evt, shell->state);
  return -1;
} // rsysd_shell_fsm


// ----------------------------------------------------------------------------
// Name   : rsysd_new_client
// Usage  : Return a free context for a new client
// Return : Context if OK
//          0, if error
// ----------------------------------------------------------------------------
static rsysd_client_t *rsysd_new_client(
                                        int sd
                                       )
{
rsysd_client_t *client;

//RSYSD_ERR("NEW CLIENT sd=%d\n", sd);

  client = (rsysd_client_t *)malloc(sizeof(rsysd_client_t));
  if (client)
  {
    client->state  = RSYSD_STATE_ALLOCATED;
    client->sd     = sd;
    client->cmd    = (char *)0;
    client->cmd_sz = 0;
    client->shell  = (rsysd_shell_t *)0;

    // Link the context
    client->next = rsysd_clients;
    client->prev = (rsysd_client_t *)0;
    if (client->next)
    {
      assert((rsysd_client_t *)0 == client->next->prev);
      client->next->prev = client;
    }

    rsysd_clients = client;

    rsysd_nb_clients ++;
  } // End if client

  return client;

} // rsysd_new_client






// ----------------------------------------------------------------------------
// Name   : rsysd_get_shell
// Usage  : Return a free shell
// Return : Context if OK
//          0, if error
// ----------------------------------------------------------------------------
static rsysd_shell_t *rsysd_get_shell(void)
{
unsigned int   i;
rsysd_shell_t *shell;

  for (i = 0; i < rsysd_nb_shells; i ++)
  {
    shell = &(rsysd_shells[i]);
    if (RSYSD_STATE_FREE == shell->state)
    {
      return shell;
    }
  } // End for

  return (rsysd_shell_t *)0;
} // rsysd_get_shell



// ----------------------------------------------------------------------------
// Name   : rsysd_read_client
// Usage  : Read data from the client process
// Return : Message type, if OK
//          -1, Error
//          -2, Disconnection
//          -3, Memory allocation problem
// ----------------------------------------------------------------------------
static int rsysd_read_client(rsysd_client_t *client)
{
rsys_msg_t msg;
int        rc;

  // Read the header
  rc = read(client->sd, &msg, sizeof(msg));

  if (rc != sizeof(msg))
  {
    if (0 == rc)
    {
      RSYSD_ERR("read(%d): End of connection\n", client->sd);
      return -2;
    }

    RSYSD_ERR("read(%d)=%d: '%m' (%d)\n", client->sd, rc, errno);
    return -1;
  }

  switch(msg.type)
  {
    case RSYS_MSG_CMD:
    {
      // Allocate the space for the data
      if (client->cmd_sz < msg.length)
      {
        client->cmd = (char *)realloc(client->cmd, msg.length);
        if (!(client->cmd))
	{
          RSYSD_ERR("realloc(%zu): '%m' (%d)\n", msg.length, errno);
          return -3;
	}

        client->cmd_sz = msg.length;
      }

      // Read the data
      rc = read(client->sd, client->cmd, msg.length);
      if (rc != (int)(msg.length))
      {
        if (0 == rc)
        {
          RSYSD_ERR("read(%d): End of connection\n", client->sd);
          return -2;
        }

        RSYSD_ERR("read(%d)=%d: '%m' (%d)\n", client->sd, rc, errno);
        return -1;
      }

      rc = RSYS_MSG_CMD;
    }
    break;

    default : 
    {
      RSYSD_ERR("Unexpected message type = %d\n", msg.type);
      rc = -1;
    }
  } // End switch

  return rc;
} // rsysd_read_client


static int rsysd_sig_received;

//---------------------------------------------------------------------------
// Name : rsysd_signal
// Usage: Signal handler
//----------------------------------------------------------------------------
static void rsysd_signal(int sig)
{
  (void)sig;

  rsysd_loop = 0;

  rsysd_sig_received = 1;

} // rsysd_signal


//---------------------------------------------------------------------------
// Name : rsysd_help
// Usage: Display help
//----------------------------------------------------------------------------
static void rsysd_help(char *prog)
{
char *p = basename(prog);

fprintf(stderr,
        "\n%s %s\n"
        "\n"
        "Copyright (C) 2007-2018  Rachid Koucha\n"
        "\n"
        "This program comes with ABSOLUTELY NO WARRANTY.\n"
        "This is free software, and you are welcome to redistribute it\n"
        "under the terms of the GNU General Public License as published by\n"
        "the Free Software Foundation; either version 3 of the License , or\n"
        "(at your option) any later version.\n"
        "\n"
        "Usage: %s [<options> ...]\n"
        "\n"
        "Options:\n"
        "\n"
        "\t-s | --shells            : Shells to run\n"
        "\t-V | --version           : Display the version\n"
        "\t-d | --debug             : Set debug level\n"
        "\t-D | --daemon            : Daemon mode\n"
        "\t-h | --help              : This help\n"
        ,
        p, rsysd_version,
        p
	);
} // rsysd_help



// ----------------------------------------------------------------------------
// Name   : main
// Usage  : Daemon's entry point
// ----------------------------------------------------------------------------
int main(int ac, char *av[])
{
unsigned int        options = 0;
int                 opt;
const char         *shells = (char *)0;
int                 rsysd_sd = -1;
int                 rc = 0;
pthread_t           tid;
fd_set              fdset;
rsysd_client_t     *client, *next_client;
rsysd_shell_t      *shell;
unsigned int        i;
struct sigaction    action;

  while ((opt = getopt_long(ac, av, "s:Vd:Dh", rsysd_longopts, NULL)) != EOF)
  {
    switch(opt)
    {
      case 's' : // Background shells to launch
      {
        shells = optarg;
        options |= 0x01;
      }
      break;

      case 'V' : // Version
      {
        options |= 0x02;
      }
      break;

      case 'd' : // Debug level
      {
        rsysd_dbg_level = atoi(optarg);
        options |= 0x04;
      }
      break;

      case 'D' : // Daemon mode
      {
        options |= 0x08;
      }
      break;

      case 'h' : // Help
      {
        rsysd_help(av[0]);
        exit(0);
      }
      break;

      case '?' :
      default :
      {
        rsysd_help(av[0]);
        exit(1);
      }
    } // End switch
  } // End while

  if (options & 0x02)
  {
    printf("RSYSTEMD version: %s\n", rsysd_version);
    exit(0);    
  }

  // If daemon mode is requested, launch the detached child right now
  // For the moment we do not redirect the input/outputs to /dev/null
  if (options & 0x08)
  {
    rc = daemon(0, 1);
    if (rc != 0)
    {
      RSYSD_ERR("daemon(): '%m' (%d)\n", errno);
      rc = 1;
      goto end;
    }

    //
    // From here the parent process exited with code 0, the child process
    // is going on...
    //
  }

  // Capture SIGTERM
  (void)sigemptyset(&(action.sa_mask));
  action.sa_flags = 0;
  action.sa_restorer = 0;
  action.sa_handler = rsysd_signal;
  rc = sigaction(SIGTERM, &action, 0);
  if (0 != rc)
  {
    RSYSD_ERR("sigaction(SIGTERM): '%m' (%d)\n", errno);
    rc = 1;
    goto end;
  }

  // Get the pathname of the socket
  rsysd_get_spath();

  // Make some cleanups
  (void)unlink(rsysd_spath);

  // Create the server socket
  rsysd_sd = rsysd_open_socket();
  if (rsysd_sd < 0)
  {
    rc = 1;
    goto end;
  }

  RSYSD_DBG(1, "Listening on socket '%s'\n", rsysd_spath);

  // Create the graceful thread
  rc = pthread_create(&tid, (const pthread_attr_t *)0, rsysd_graceful_thd, (void *)0);
  if (0 != rc)
  {
    errno = rc;
    RSYSD_ERR("pthread_create(): '%m' (%d)\n", errno);
    rc = 1;
    goto end;
  }

  // Fork/exec the configured shells (By default only one shell running on
  // all the CPUs)
  // The configuration is passed either as parameter (-s option) or
  // through RSYSD_SHELLS environment variable
  rc = rsysd_create_shells(shells);
  if (rc != 0)
  {
    rc = 1;
    goto end;
  }

  // Put the server's socket in the set of file descriptors to wait on
  FD_ZERO(&rsysd_fdset);
  rsysd_fd_max = -1;
  rsysd_add_set(rsysd_sd, &rsysd_fdset, &rsysd_fd_max);

  // Add the file descriptors of the shells into the set
  for (i = 0; i < rsysd_nb_shells; i++)
  {
    rsysd_add_set(rsysd_shells[i].fd, &rsysd_fdset, &rsysd_fd_max);
  } // End for shells  

  // Forever loop to wait for the events 
  while (rsysd_loop)
  {
    fdset = rsysd_fdset;

    rc = select(rsysd_fd_max + 1, &fdset, NULL, NULL, NULL);
    switch(rc)
    {
      case -1:
      {
        if (EINTR == errno)
	{
          // Loop again

          break;
	}

	RSYSD_ERR("select(): '%m' (%d)\n", errno);
	rc = 1;
        goto end;
      }
      break;

      default: // At least one file 
      {
      int nb_ready = rc;

        // If it is a new client connection
        if (FD_ISSET(rsysd_sd, &fdset))
	{
	int                 sd;
        struct sockaddr_un  addr;
        socklen_t           laddr;

          nb_ready --;

          // Accept the connection
          laddr = sizeof(addr);
          if (-1 == (sd = accept(rsysd_sd, (struct sockaddr *)&addr, &laddr)))
	  {
	    RSYSD_ERR("accept(%d): '%m' (%d)\n", rsysd_sd, errno);
            rc = 1;
            goto end;
	  }

          // Create a client context
          client = rsysd_new_client(sd);
          if (client)
	  {
            // Add the client's socket in the list
            rsysd_add_set(client->sd, &rsysd_fdset, &rsysd_fd_max);
	  }
          else
	  {
            // Memory allocation pb
            RSYSD_ERR("new_client(%d): '%m' (%d)\n", rsysd_sd, errno);
            (void)rsys_send_msg(sd, RSYS_MSG_OOM);
            rsysd_graceful_close(sd);
	  }
	} // End if client connection

        // Data from shells ?
        for (i = 0; (i < rsysd_nb_shells) && (nb_ready > 0); i++)
	{
          shell = &(rsysd_shells[i]);
          if (FD_ISSET(shell->fd, &fdset))
	  {
            nb_ready --;

            rc = rsysd_shell_fsm(shell, RSYSD_EVT_DATA, 0);

	  } // End if data from this shell
        } // End for shells

        // Commands from clients ?
        client = rsysd_clients;
        while (client && (nb_ready > 0))
	{
          if (FD_ISSET(client->sd, &fdset))
	  {
            nb_ready --;

            // If the client is not deleted and is not linked to a shell,
            // link it
            if ((RSYSD_STATE_ALLOCATED == client->state) && !(client->shell))
	    {
              // Look for an available shell
              shell = rsysd_get_shell();
              if (shell)
	      {
                rsysd_shell_fsm(shell, RSYSD_EVT_LINK, client);
	      }
            } // End if client not linked

            // Read the command line sent by the client
            rc = rsysd_read_client(client);

            // If the client is linked
            if (client->shell)
	    {
              switch(rc)
	      {
  	        case RSYS_MSG_CMD : // Command line
	        {
                  rc = rsysd_shell_fsm(client->shell, RSYSD_EVT_CMD, 0);
	        }
                break;

  	        case -1: // Error
  	        case -2: // Disconnection
	        case -3: // Memory allocation pb
	        default : // Error
	        {
                  // Disconnection from the client
                  rc = rsysd_shell_fsm(client->shell, RSYSD_EVT_CLIENT_DCNX, 0);
	        }
                break;
	      } // End switch
	    }
            else // Client not linked
	    {
              switch(rc)
	      {
  	        case 0 : // Command line
	        {
                  // Ignore the received message
                  RSYSD_ERR("No available shell ==> Ignoring cmd '%s' from client %d\n", client->cmd, client->sd);
                  (void)rsys_send_msg(client->sd, RSYS_MSG_BUSY);
	        }
                break;

  	        case -1: // Error
  	        case -2: // Disconnection
	        case -3: // Memory allocation pb
	        default : // Error
	        {
                  // Delete the client's context
                  rsysd_delete_client(client);
	        }
                break;
	      } // End switch
	    } // End if client is linked

            // We keep the client's socket in the set as we may receive
            // a disconnection or any further command from it

	  } // End if data from this client

          next_client = client->next;

          // If the current client has been deleted
          if (RSYSD_STATE_FREE == client->state)
	  {
            // Free it
            rsysd_free_client(client);
	  }

          client = next_client;
        } // End for clients

        assert(0 == nb_ready);
      }
      break;

    } // End switch(rc)

  } // End while

end:

  if (rsysd_sd >= 0)
  {
    close(rsysd_sd);
    rsysd_sd = -1;
  }

  // Remove the socket file
  (void)unlink(rsysd_spath);

  // Terminate the shells
  rsysd_delete_shells();

  if (rsysd_sig_received)
  {
    // Specific exit code to show that a signal stopped the server
    rc = 2;
  }

  RSYSD_DBG(1, "Exiting with code %d\n", rc);

  return rc;
} // main

