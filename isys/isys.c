// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : isys.c
// Description : system() service based on PDIP and a remanent background
//               shell to avoid fork/exec() of the current process
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
//     20-Dec-2017 R. Koucha    - Creation
//     28-Feb-2018 R. Koucha    - Management of fork()
//                              - Added PDIP_FLAG_RECV_ON_THE_FLOW
//                              - Added ISYS_TIMEOUT env variable
//     29-Mar-2018 R. Koucha    - Configure PDIP with external SIGCHLD handler
//     05-Apr-2018 R. Koucha    - isystem(): No dynamic allocation for short
//                                command lines
//                              - No longer ignore SIGINT/SIGQUIT in the shell
//                                as system() ignores those signals in the
//                                father process while running the command (not
//                                in the shell!)
//                                ==> Here we decide not to manage those
//                                signals at all as it is not adapted to
//                                multithreaded applications
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#define _GNU_SOURCE   // For sched.h

#include <pthread.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <pdip.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "isys.h"


// ----------------------------------------------------------------------------
// Name   : ISYS_ERR
// Usage  : Error messages
// ----------------------------------------------------------------------------
#define ISYS_ERR(format, ...) do {                               \
    fprintf(stderr,                                              \
            "ISYS ERROR(%d) - (%s#%d): "format,                  \
            getpid(), __FUNCTION__, __LINE__, ## __VA_ARGS__);   \
                                 } while (0)



// ----------------------------------------------------------------------------
// Name   : isys_nb_cpu
// Usage  : Number of CPUs
// ----------------------------------------------------------------------------
static unsigned int isys_nb_cpu;


// ----------------------------------------------------------------------------
// Name   : isys_pdip
// Usage  : PDIP object which runs the shell
// ----------------------------------------------------------------------------
static pdip_t isys_pdip;


// ----------------------------------------------------------------------------
// Name   : ISYS_SH_PATH
// Usage  : Shell pathname
// ----------------------------------------------------------------------------
#define ISYS_SH_PATH "/bin/sh"


// ----------------------------------------------------------------------------
// Name   : ISYS_SH_PROMPT
// Usage  : Redefined shell prompt
// ----------------------------------------------------------------------------
#define ISYS_SH_PROMPT "ISYS_PDIP> "


// ----------------------------------------------------------------------------
// Name   : isys_display...
// Usage  : Buffer into which the shell prints its outputs
// ----------------------------------------------------------------------------
static char   *isys_display;
static size_t  isys_display_sz;
static size_t  isys_data_sz;


// ----------------------------------------------------------------------------
// Name   : isys_timeout
// Usage  : Timeout to receive data from the shell
// ----------------------------------------------------------------------------
static struct timeval isys_timeout;


// ----------------------------------------------------------------------------
// Name   : isys_recv_status
// Usage  : Receive the status of the last command
// Return : PDIP_RECV_...
// ----------------------------------------------------------------------------
static int isys_recv_status(
                            int            *status
                           )
{
int             rc;
char            status_display[10];
size_t          offset = 0;
char           *p;
struct timeval  to;

  status_display[0] = '\0';

  do
  {
    // Refresh the timeout
    to = isys_timeout;
    rc = pdip_recv(isys_pdip, "^" ISYS_SH_PROMPT "$", &isys_display, &isys_display_sz, &isys_data_sz, &to);
    switch(rc)
    {
      case PDIP_RECV_FOUND:
      {
        p = isys_display + isys_data_sz - sizeof(ISYS_SH_PROMPT);

        //fprintf(stderr, "p='%s'\n", p);

        // If there are data displayed before the prompt
        if (p > isys_display)
        {
          // Patch the byte right before the prompt which must be a '\n'
          if ('\n' != *p)
	  {
            ISYS_ERR("Incoherent display from the shell: are we desynchronized?!? isys_data_sz=%zu, isys_display_sz=%zu, isys_display='%s'\n", isys_data_sz, isys_display_sz, isys_display);
            return PDIP_RECV_ERROR;
	  }

          // Patch the LF
          *p = '\0';

          rc = snprintf(status_display + offset, sizeof(status_display) - offset, "%s", isys_display);
          if (rc >= (int)(sizeof(status_display) - offset))
	  {
            ISYS_ERR("Too much data for the command status ?!? isys_data_sz=%zu, isys_display_sz=%zu, isys_display='%s'\n", isys_data_sz, isys_display_sz, isys_display);
            return PDIP_RECV_ERROR; 
	  }
        }
        else
	{
          // There must be at least 2 bytes in the buffer
          if (!(status_display[0]) || ('\n' != status_display[offset - 1]))
	  {
            ISYS_ERR("Incoherent display from the shell: are we desynchronized?!? isys_data_sz=%zu, isys_display_sz=%zu, isys_display='%s', status='%s'\n", isys_data_sz, isys_display_sz, isys_display, status_display);
            return PDIP_RECV_ERROR;
	  }

          // Patch the LF
          status_display[offset - 1] = '\0';
	}

        // Translate the status into an integer
        *status = atoi(status_display);

        // According to my experimentations, $? is:
        //   . The exit code if the program exited
        //   . (0x80 | signal_number) if the program terminated with a signal 
        //   . 127 (= 0x7F) if the program could not be executed by the shell

        // If it is a signal number
        if (*status & 0x80)
        {
          // Keep it as it is
        }
        else // Exit code
        {
          (*status) <<= 8;
        }
       
        return PDIP_RECV_FOUND;
      }
      break;

      case PDIP_RECV_DATA:
      {
        rc = snprintf(status_display + offset, sizeof(status_display) - offset, "%s", isys_display);
        if (rc >= (int)(sizeof(status_display) - offset))
	{
          ISYS_ERR("Too much data for the command status ?!? isys_data_sz=%zu, isys_display_sz=%zu, isys_display='%s'\n", isys_data_sz, isys_display_sz, isys_display);
          return PDIP_RECV_ERROR; 
	}

        offset += rc;
      }
      break;

      case PDIP_RECV_TIMEOUT:
      case PDIP_RECV_ERROR:
      default:
      {
        return rc;
      }
      break;
    } // End switch
  } while(1);

} // isys_recv_status



// ----------------------------------------------------------------------------
// Name   : isys_recv_data
// Usage  : Receive data until the prompt appears. The intermediate data
//          is displayed on the standard output if requested
// Return : PDIP_RECV_...
// ----------------------------------------------------------------------------
static int isys_recv_data(
                          int             out
			 )
{
int             rc;
struct timeval  to;

  do
  {
    // Refresh the timeout
    to = isys_timeout;

    rc = pdip_recv(isys_pdip, "^" ISYS_SH_PROMPT "$", &isys_display, &isys_display_sz, &isys_data_sz, &to);
    switch(rc)
    {
      case PDIP_RECV_FOUND:
      {
        if (out)
	{
	char *p;

  	  // We must have received at leat the prompt
	  if ((sizeof(ISYS_SH_PROMPT) - 1) > isys_data_sz)
	  {
            ISYS_ERR("Incoherent display from the shell: are we desynchronized?!? isys_data_sz=%zu, isys_display_sz=%zu, isys_display='%s'\n", isys_data_sz, isys_display_sz, isys_display);
            return PDIP_RECV_ERROR;
	  }

          p = isys_display + isys_data_sz - sizeof(ISYS_SH_PROMPT);

          // If there are data displayed before the prompt
          if (p > isys_display)
          {
            // Patch the first byte of the prompt to display exactly the 
            // program's output right before the prompt
            p++;
            *p = '\0';
            printf("%s", isys_display);
          }
	}

        return PDIP_RECV_FOUND;
      }
      break;

      case PDIP_RECV_DATA:
      {
        if (out && isys_data_sz)
	{
          printf("%s", isys_display);
	}
      }
      break;

      case PDIP_RECV_TIMEOUT:
      case PDIP_RECV_ERROR:
      default:
      {
        return rc;
      }
      break;
    } // End switch
  } while(1);

} // isys_recv_data



//----------------------------------------------------------------------------
// Name        : isystem
// Description : Interactive system() service (submit the command
//               interactively to a running background shell) 
// Return      : status of the executed command, if OK
//               -1, if error (errno is set)
//----------------------------------------------------------------------------
int isystem(
            const char *fmt, ...
           )
{
int           rc;
int           status;
va_list       ap;
char         *cmd;
unsigned int  i;
char          cmd_short[256];

  if (!isys_pdip)
  {
    ISYS_ERR("No running shell!\n");
    return -1;
  }

  // According to system(3) manual, if the command exists, the status indicates
  // if the shell is available (in the GLIBC source code, an "exit 0" is run
  // to feed the shell
  if (!fmt)
  {
    // If we are here, this means that the shell exists as it is started by
    // the entry point of this library
    return 0;
  }

  // Make the command line
  // We first try to format the command in a local buffer to avoid dynamic
  // allocation
  va_start(ap, fmt);
  cmd = cmd_short;
  rc = vsnprintf(cmd, sizeof(cmd_short), fmt, ap);
  va_end(ap);

  // If the buffer is too short
  if (rc >= (int)sizeof(cmd_short))
  {
    // Retry with dynamic allocation
    va_start(ap, fmt);
    cmd = (char *)0;
    rc = vasprintf(&cmd, fmt, ap);
    va_end(ap);

    if (-1 == rc)
    {
      // Error
      ISYS_ERR("vasprintf(%s): '%m' (%d)\n", fmt, errno);
      status = -1;
      goto err;
    }
  }

  if (0 == rc)
  {
    // Same return code as if the command was empty
    status = 0;
    goto err;
  }

  // When there are line feeds at the end of the command line,
  // we get multiple prompts on the same line since the echo is deactivated
  // This perturbates the reception of the ending prompt as the regex is a prompt
  // at the beginning of line and nothing behind it: "^ISYS_PDIP> $"
  //
  // PDIP(5808-2) - pdip_look_for_regex#655: Looking for a match of <^ISYS_PDIP> $> in (0x55647dda4560):
  // 000055647DDA4560 49 53 59 53 5F 50 44 49 50 3E 20 49 53 59 53 5F *ISYS_PDIP> ISYS_*
  // 000055647DDA4570 50 44 49 50 3E 20 49 53 59 53 5F 50 44 49 50 3E *PDIP> ISYS_PDIP>*
  // 000055647DDA4580 20 49 53 59 53 5F 50 44 49 50 3E 20 49 53 59 53 * ISYS_PDIP> ISYS*
  // 000055647DDA4590 5F 50 44 49 50 3E 20                            *_PDIP>          *
  //
  // So, we remove them here... But there are several other cases of command line configuration which would
  // make the pattern matching fail. The user must be cautious.
  i = rc - 1;
  while (isspace(cmd[i]))
  {
    i --;
  } // End while
  cmd[i + 1] = '\0';

  // Send the command
  rc = pdip_send(isys_pdip, "%s\n", cmd);
  if (rc < 0)
  {
    ISYS_ERR("pdip_send(%s): '%m' (%d)\n", cmd, errno);
    status = -1;
    goto err;
  }

  // Wait for the prompt
  rc = isys_recv_data(1);
  if (rc != PDIP_RECV_FOUND)
  {
    ISYS_ERR("pdip_recv(%s): Didn't received the shell prompt (rc = %d) ?!?\n", cmd, rc);
    status = -1;
    goto err;
  }

  // Send the status request
  rc = pdip_send(isys_pdip, "%s\n", "echo $?");
  if (rc < 0)
  {
    ISYS_ERR("pdip_send(%s): '%m' (%d)\n", cmd, errno);
    status = -1;
    goto err;
  }

  // Receive the status
  status = 0;
  rc = isys_recv_status(&status);
  if (rc != PDIP_RECV_FOUND)
  {
    ISYS_ERR("No status: are we desynchronized?!? isys_data_sz=%zu, isys_display_sz=%zu, isys_display='%s', cmd='%s'\n", isys_data_sz, isys_display_sz, isys_display, cmd);
    status = -1;
    goto err;
  }

err:

  // Free the resources
  if (cmd && (cmd != cmd_short))
  {
    free(cmd);
  }

  return status;
} // isystem




static void isys_child_fork(void)
{
  // The child process inherited the father's PDIP context
  // but this context references a controlled process which
  // is linked to the father (i.e. father's child)
  // We need to clear the context without killing the
  // controlled process as the father needs it !
  // ==> PDIP library clears all its contexts and deactivate
  //     its signal handler upon fork()

  // Make sure that isystem() will fail without initialization
  isys_pdip = (pdip_t)0;
  isys_nb_cpu = 0;

  // If one still needs the ISYS service in a child process,
  // isys_lib_initialize() must be called explicitely

} // isys_child_fork




// ----------------------------------------------------------------------------
// Name   : isys_lib_initialize
// Usage  : Library initialization when needed in a child process
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int isys_lib_initialize(void)
{
char           *av[2];
int             rc;
pdip_cfg_t      cfg;
long            rcl;
char           *env;

#define ISYS_DEFAULT_TIMEOUT    10   // seconds

  // Timeout for data from the shell
  env = getenv(ISYS_ENV_TIMEOUT);
  if (env)
  {
    isys_timeout.tv_sec = atoi(env);
  }
  else
  {
    isys_timeout.tv_sec = ISYS_DEFAULT_TIMEOUT;
  }

  // Get the number of CPUs
  rcl = sysconf(_SC_NPROCESSORS_ONLN);
  if (rcl < 0)
  {
    ISYS_ERR("sysconf(): '%m' (%d)\n", errno);
    return -1;
  }

  isys_nb_cpu = rcl;

  // Configure the PDIP library
  rc = pdip_configure(0, 0);
  if (0 != rc)
  {
    ISYS_ERR("pdip_configure(): '%m' (%d)\n", errno);
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
  // We also activate reception on the flow to receive data as it arrives
  // instead of one big chunk of memory at the end of the process
  cfg.flags = PDIP_FLAG_ERR_REDIRECT | PDIP_FLAG_RECV_ON_THE_FLOW;
  isys_pdip = pdip_new(&cfg);
  if (!isys_pdip)
  {
    ISYS_ERR("pdip_new(): '%m' (%d)\n", errno);
    return -1;
  }

  //(void)pdip_set_debug_level(isys_pdip, 10);

  // Set the prompt of the shell
  rc = setenv("PS1", ISYS_SH_PROMPT, 1);
  if (0 != rc)
  {
    ISYS_ERR("setenv(PS1): '%m' (%d)\n", errno);
    return -1;
  }

  av[0] = ISYS_SH_PATH;
  av[1] = (char *)0;
  rc = pdip_exec(isys_pdip, 1, av);
  if (rc < 0)
  {
    ISYS_ERR("pdip_exec(): '%m' (%d)\n", errno);
    return -1;
  }

  // Wait for the 1st prompt
  rc = isys_recv_data(0);
  if (rc != PDIP_RECV_FOUND)
  {
    ISYS_ERR("pdip_recv(): Didn't received the prompt (rc = %d)?!?\n", rc);
    return -1;
  }

  // Disable the echo of the characters to make the transactions faster
  rc = pdip_send(isys_pdip, "%s\n", "stty -echo");
  if (rc < 0)
  {
    ISYS_ERR("pdip_send(): '%m' (%d)\n", errno);
    return -1;
  }

  // Wait for the prompt
  rc = isys_recv_data(0);
  if (rc != PDIP_RECV_FOUND)
  {
    ISYS_ERR("pdip_recv(): Didn't received the prompt (rc = %d)?!?\n", rc);
    return -1;
  }

  rc = pthread_atfork(0, 0, isys_child_fork);
  if (0 != rc)
  {
    errno = rc;
    ISYS_ERR("pthread_atfork(): '%m' (%d)\n", errno);
    return -1;
  }

  return 0;
} // isys_lib_initialize



static void __attribute__ ((constructor)) isys_lib_init(void);

// ----------------------------------------------------------------------------
// Name   : isys_lib_init
// Usage  : Library entry point
// ----------------------------------------------------------------------------
static void isys_lib_init(void)
{

  (void)isys_lib_initialize();

} // isys_lib_init


static void __attribute__ ((destructor)) isys_lib_exit(void);

// ----------------------------------------------------------------------------
// Name   : rsys_lib_exit
// Usage  : Library exit point
// ----------------------------------------------------------------------------
static void isys_lib_exit(void)
{
int status;

  // Delete the PDIP object
  (void)pdip_delete(isys_pdip, &status);

} // isys_lib_exit



