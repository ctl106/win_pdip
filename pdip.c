// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : pdip.c
// Description : Programmed Dialogue with Interactive Programs
// License     :
//
//  Copyright (C) 2007-2018 Rachid Koucha <rachid dot koucha at gmail dot com>
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
//     12-Jun-2007  R. Koucha    - Creation
//     03-Jul-2007  R. Koucha    - Added DOS mode to send CR/LF at end
//                                 of line instead of LF
//     16-Jul-2007  R. Koucha    - Better management of end of file on
//                                 input
//                               - Reset the signals before exiting
//     18-Jul-2007  R. koucha    - Added "sleep" command
//     03-Aug-2007  R. Koucha    - Enhanced the way we wait for the
//                                 child's status because we were
//                                 systematically calling "kill_chld"
//                                 which triggered sleeps
//     19-Sep-2007 R. Koucha     - The data coming from the program may
//                                 contain NUL chars (e.g. telnet login
//                                 session). So, when displaying the
//                                 data and searching synchro strings, we
//                                 ignore the NUL chars.
//                               - Suppress useless DOS mode
//                               - Better management of the synchro
//                                 strings
//     02-Oct-2007 R. Koucha     - Management of regular expressions for
//                                 for 'recv' command
//                               - Management of formated strings for
//                                 'send' command
//     16-Oct-2007 R. Koucha     - Suppressed useless -v = verbose
//                               - Added -V = version
//     19-Oct-2007 R. Koucha     - Fixed pb of failed exec program
//                                 (cf. comment above the definition
//                                 of pdip_chld_failed_on_exec)
//     25-Nov-2007 R. Koucha     - Fixed management of offset in
//                                 pdip_read_line()
//     14-Dec-2007 R. Koucha     - Support of POSIX regular expressions
//                               - Exit 1 on timeout
//                               - Better management of the string
//                                 parameters to allow '#' and \" inside*
//                                 them
//                               - Command to launch directly on the
//                                 command line
//     18-Jan-2008 R. Koucha     - Fixed infinite loop in pdip_write()
//                                 when output file descriptor becomes
//                                 invalid (e.g. remote program crashed)
//                                 and so, write() returns -1
//                               - errno saving
//     21-Apr-2008 R. Koucha     - Mngt of exception signals
//     24-Apr-2008 R. Koucha     - Suppressed the debug print in SIGCHLD
//                                 handler
//     06-Feb-2009 R. Koucha     - pdip_handle_synchro(): do not skip blank
//                                 chars !
//                               - Added ability to send ESC = 'CTRL [' control
//                                 character
//     10-Feb-2009 R. Koucha     - Fixed parameter mngt of sleep keyword
//     19-Aug-2009 R. Koucha     - Added 'print' command
//                               - Added 'dbg' command with debug level
//                               - Fixed pdip_write() because it didn't work
//                                 when multiple writes were necessary
//                               - pdip_read_program(): Replaced polling by a
//                                 timed wait
//                               - Better management of the end of the program
//                               - Dynamic allocation of the internal buffers
//                                 to accept very long lines
//     29-Nov-2009 R. Koucha     - Do not redirect the error output of the
//                                 child unless "-e" option is specified
//     25-Dec-2009 R. Koucha     - Management of background mode
//                               - Added -t option
//     29-Jul-2010 R. Koucha     - Made the slave side of the PTY become the
//                                 controlling terminal of the controlled
//                                 program
//                               - Added management of the control characters
//                                 through the notation "^character"
//                               - Added new keyword "sig" to send signals to
//                                 the controlled program
//     18-Aug-2010 R. Koucha     - Added option "-p" to propagate exit code of
//                                 controlled program (no longer the default
//                                 behaviour !)
//     10-Sep-2012 EMarchand     - Fixed some new gcc4.4 warnings
//     05-Jun-2013 R. Koucha     - Added 'sh' command
//     08-Oct-2013 R. Koucha     - Display (flush) outstanding data when
//                                 program finishes prematurely
//                               - Default buffer size 512 --> 4096
//     29-Jan-2015 S. Dubois     - Replaced memcpy() by memmove() to support
//                                 overlaps
//     04-Jun-2015 R. Koucha     - Added -R option to trigger reads in
//                                 background in order to avoid pseudo-terminal
//                                 saturation which would block the child
//                                 process on a write() system call
//     01-Nov-2017 R. Koucha     - Added REG_NEWLINE in regcomp() call to
//                                 make '$' wildcard work for end of lines
//     06-Nov-2017 R. Koucha     - Added libpdip.so
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#define _GNU_SOURCE
#include <getopt.h>
//#include <sys/wait.h>
#include <libgen.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
//#include <sys/select.h>
#include <signal.h>
#include <stdlib.h>
//include <regex.h>
#include <time.h>
//#include <termios.h>
//#include <sys/ioctl.h>

#include "config.h"
#include "pdip_util.h"

#include "ptypes.h"



// ----------------------------------------------------------------------------
// Name   : pdip_debug
// Usage  : Debug level
// ----------------------------------------------------------------------------
static int pdip_debug;

// ----------------------------------------------------------------------------
// Name   : PDIP_ERR
// Usage  : Error messages
// ----------------------------------------------------------------------------
#define PDIP_ERR(format, ...) do { if (!pdip_background_out)	        \
      fprintf(stderr,                                                   \
              "PDIP(%d) ERROR (%s#%d): "format,                         \
              getpid(), __FUNCTION__, __LINE__, ## __VA_ARGS__);        \
                                 } while (0)

// ----------------------------------------------------------------------------
// Name   : pdip_in
// Usage  : Input of PDIP (terminal or script)
// ----------------------------------------------------------------------------
static int pdip_in;

// ----------------------------------------------------------------------------
// Name   : pdip_out
// Usage  : Output of PDIP (terminal)
// ----------------------------------------------------------------------------
static int pdip_out;

// ----------------------------------------------------------------------------
// Name   : pdip_err
// Usage  : Error output of PDIP (terminal)
// ----------------------------------------------------------------------------
static int pdip_err;

// ----------------------------------------------------------------------------
// Name   : pdip_pty
// Usage  : Master side of the pseudo-terminal
// ----------------------------------------------------------------------------
static int pdip_pty = -1;

// ----------------------------------------------------------------------------
// Name   : pdip_background_in
// Usage  : Set to 1 if background mode and input is the terminal
// ----------------------------------------------------------------------------
static int pdip_background_in;

// ----------------------------------------------------------------------------
// Name   : pdip_background_out
// Usage  : Set to 1 if background mode and output is the terminal
// ----------------------------------------------------------------------------
static int pdip_background_out;

// ----------------------------------------------------------------------------
// Name   : pdip_synchro
// Usage  : Current synchronization string
// ----------------------------------------------------------------------------
static char pdip_synchro[128];

// ----------------------------------------------------------------------------
// Name   : environ
// Usage  : Pointer on the environment variables
// ----------------------------------------------------------------------------
extern char **environ;

// ----------------------------------------------------------------------------
// Name   : PDIP_COMMENT
// Usage  : Comment marker on the command line
// ----------------------------------------------------------------------------
#define PDIP_COMMENT  '#'


// ----------------------------------------------------------------------------
// Name   : PDIP_DBG/DUMP
// Usage  : Debug messages
// ----------------------------------------------------------------------------
#define PDIP_DBG(level, format, ...) do { if (!pdip_background_out && (pdip_debug >= level)) \
                                      fprintf(stderr,                                  \
                                              "\nPDIP_DBG(%d-%d) - %s#%d: "format,     \
                                              getpid(), level,                         \
                                              __FUNCTION__, __LINE__, ## __VA_ARGS__); \
                                 } while(0)

#define PDIP_DUMP2(level, b, l) do { if (!pdip_background_out && (pdip_debug >= level)) \
                                    {  pdip_dump((b), (l)); }                    \
                                  } while(0)

// ----------------------------------------------------------------------------
// Name   : pdip_version
// Usage  : Version of the software
// ----------------------------------------------------------------------------
static const char *pdip_version = PDIP_VERSION;

// ----------------------------------------------------------------------------
// Name   : pdip_bufsz
// Usage  : I/O buffer size
// ----------------------------------------------------------------------------
static unsigned int pdip_bufsz;
#define PDIP_DEF_BUFSZ   4096


// ----------------------------------------------------------------------------
// Name   : pdip_buf
// Usage  : I/O buffer
// ----------------------------------------------------------------------------
static char *pdip_buf;
static char *pdip_buf1;

// ----------------------------------------------------------------------------
// Name   : pdip_outstanding_buf
// Usage  : Buffer of data in which the synchro string is looked for before
//          being printed out
// ----------------------------------------------------------------------------
static char         *pdip_outstanding_buf;
static unsigned int  pdip_outstanding_buf_sz; // Current buffer size
static unsigned int  pdip_loutstanding;       // Current write offset

// ----------------------------------------------------------------------------
// Name   : pdip_backread
// Usage  : Flag triggering the reading of the incoming data even if no read
//          action is on track
// ----------------------------------------------------------------------------
static int pdip_backread;


// ----------------------------------------------------------------------------
// Name   : pdip_argv/argc/argv_nb
// Usage  : Parameter table
// ----------------------------------------------------------------------------
static char **pdip_argv    = NULL;
static int    pdip_argc    = 0;
static int    pdip_argv_nb = 0;


// ----------------------------------------------------------------------------
// Name   : pdip_pid
// Usage  : Process id of the piloted child
// ----------------------------------------------------------------------------
static pid_t pdip_pid = -1;


// ----------------------------------------------------------------------------
// Name   : pdip_dead_prog
// Usage  : Set to one when the sub-process (program) dies
// ----------------------------------------------------------------------------
static int pdip_dead_prog = 1;

// ----------------------------------------------------------------------------
// Name   : pdip_exit_prog
// Usage  : Exit code of the dead program to propagate to PDIP
// ----------------------------------------------------------------------------
static int pdip_exit_prog = 0;


// ----------------------------------------------------------------------------
// Name   : pdip_chld_failed_on_exec
// Usage  : 0, if child OK
//          1, if child failed on exec while father was forking
// Note   :
//          If the child fails because of a bad pathname for the name
//          of the program to exec, fork() will be interrupted in the
//          father and so, pdip_pid will not be assigned before going
//          into the SIGCHLD handler.
//          Moreover, fork() does not return any error but the pid
//          of the terminated process !!!! Hence, the global variable
//          pdip_chld_failed_on_exec that we set in the signal handler
//          if we face this situation.
//
//          I discovered this behaviour because my program was hanging. I
//          launched the debugger to attach the process and the call stack
//          showed that I was in the assert(pdip_pid == pid) of the signal
//          handler: pdip_pid was still with -1 value whereas pid (the return
//          code of waitpid() was the process id of the dead child)
// ----------------------------------------------------------------------------
static int pdip_chld_failed_on_exec = 0;

// ----------------------------------------------------------------------------
// Name   : pdip_longops
// Usage  : Option on the command line
// ----------------------------------------------------------------------------
static struct option pdip_longopts[] =
{
  { "script",     required_argument, NULL, 's' },
  { "bufsz",      required_argument, NULL, 'b' },
  { "debug",      required_argument, NULL, 'd' },
  { "version",    no_argument,       NULL, 'V' },
  { "error",      no_argument,       NULL, 'e' },
  { "term",       no_argument,       NULL, 't' },
  { "outstand",   no_argument,       NULL, 'o' },
  { "propexit",   no_argument,       NULL, 'p' },
  { "backread",   no_argument,       NULL, 'R' },
  { "help",       no_argument,       NULL, 'h' },

  // Last entry
  {NULL, 0, NULL, 0 }
};


//---------------------------------------------------------------------------
// Name : pdip_help
// Usage: Display help
//----------------------------------------------------------------------------
static void pdip_help(char *prog)
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
        "Usage: %s [<options> ...] -- <command> <params and/or options>...\n"
        "\n"
        "The command along with its parameters and options is launched and piloted by\n"
        "%s according to the commands from the script. The input and outputs\n"
        "of the program are redirected to %s.\n"
        "\n"
        "Options:\n"
        "\n"
        "If options are passed to %s and/or the command, then the command to launch\n"
        "must be separated from the options with a double hyphen (--).\n"
        "\n"
        "\t-s | --script            : Script of commands (default stdin)\n"
        "\n"
        "\t\tThe accepted commands are:\n"
        "\t\t  # ...              -- # and the following words up to the end of\n"
        "\t\t                        line are ignored (used for comments)\n"
        "\t\t  timeout x          -- Set to x seconds the maximum time to wait\n"
        "\t\t                        on each following commands\n"
        "\t\t                        (the value 0 cancels the timeout, this is\n"
        "\t\t                        the default)\n"
        "\t\t  recv \"w1 w2...\"    -- Wait for a line with the pattern \"w1 w2...\"\n"
        "\t\t                        from the program\n"
        "\t\t  send \"w1 w2...\"    -- Send the string \"w1 w2...\" to the program\n"
        "\t\t  sig SIGNAME        -- Send the signal SIGNAME to the program\n"
        "\t\t  sleep x            -- Stop activity during x seconds\n"
        "\t\t  exit               -- Terminate PDIP\n"
        "\t\t  print \"w1 w2...\"   -- Display the string \"w1 w2...\" on standard output\n"
        "\t\t  dbg level          -- Set the debug level\n"
        "\t\t  sh [-s] cmd par... -- Launch the shell command \"cmd par...\" (synchronously\n"
        "\t\t                        if '-s' option is passed)\n"
        "\n"
        "\t-b | --bufsz             : Size in bytes of the internal I/O buffer (default: %u)\n"
        "\t-d level | --debug=level : Set debug mode to 'level'\n"
        "\t-V | --version           : Display the version\n"
        "\t-e | --error             : Redirect error output of the controlled program\n"
        "\t-t | --term              : Terminal mode\n"
        "\t-o | --outstand          : Dump outstanding data at the end of the session\n"
        "\t-p | --propexit          : Propagate the exit code of the controlled program\n"
        "\t-R | --backread          : Read incoming data in background when no read command on track\n"

        "\t-h | --help              : This help\n"
        ,
        p, pdip_version,
        p,
        p,
        p,
        p,
        PDIP_DEF_BUFSZ
	);
} // pdip_help



// ----------------------------------------------------------------------------
// Name   : pdip_reset_argv
// Usage  : Reset the parameter table for future use
// Return : None
// ----------------------------------------------------------------------------
static void pdip_reset_argv(void)
{
  assert(pdip_argv_nb >= 0);
  pdip_argc = 0;
  if (pdip_argv_nb)
  {
    pdip_argv[0] = NULL;
  }
  else
  {
    assert(NULL == pdip_argv);
  }
} // pdip_reset_argv


// ----------------------------------------------------------------------------
// Name   : pdip_free_argv
// Usage  : Deallocate the parameter table
// Return : None
// ----------------------------------------------------------------------------
static void pdip_free_argv(void)
{
  assert(pdip_argv_nb >= 0);
  if (pdip_argv_nb)
  {
    free(pdip_argv);
  }
  pdip_argv = NULL;
  pdip_argc = 0;
  pdip_argv_nb = 0;
} // pdip_free_argv


// ----------------------------------------------------------------------------
// Name   : pdip_new_argv
// Usage  : Assign a new entry in the parameter table
// Return : None
// ----------------------------------------------------------------------------
static void pdip_new_argv(char *str)
{
  assert(pdip_argv_nb >= 0);

  if (pdip_argv_nb)
  {
    assert(pdip_argv_nb > pdip_argc);

    pdip_argv[pdip_argc] = str;
    pdip_argc ++;
    if (pdip_argc == pdip_argv_nb)
    {
      pdip_argv_nb ++;
      pdip_argv = (char **)realloc(pdip_argv, ((size_t)pdip_argv_nb * sizeof(char *)));
      if (!pdip_argv)
      {
        PDIP_ERR("Error %d at realloc(%"PRISIZE"u)\n", errno, (size_t)pdip_argv_nb * sizeof(char *));
        exit(1);
      }
    }
  }
  else
  {
    assert(0 == pdip_argc);
    assert(NULL == pdip_argv);

    pdip_argv_nb = 2;
    pdip_argc = 1;
    pdip_argv = (char **)malloc((size_t)pdip_argv_nb * sizeof(char *));
    if (!pdip_argv)
    {
      PDIP_ERR("Error %d at malloc(%"PRISIZE"u)\n", errno, (size_t)pdip_argv_nb * sizeof(char *));
      exit(1);
    }
    pdip_argv[0] = str;
  }
  pdip_argv[pdip_argc] = NULL;
} // pdip_new_argv


// ----------------------------------------------------------------------------
// Name   : pdip_split_cmdline
// Usage  : Make the parameter table from a command line
// Return : 0, if OK
//         -1, if error
// ----------------------------------------------------------------------------
static int pdip_split_cmdline(char *cmd_line)
{
char *p = cmd_line;
char *pString;

  // Skip heading blanks
  while (isspace(*p))
  {
    p ++;
  } // End while

  // Empty line
  if (!(*p) || (PDIP_COMMENT == *p))
  {
    return 0;
  }

  // First word is the command: [_a-zA-Z]+[_a-zA-Z0-9]*
  if ((*p != '_')               &&
      !(*p >= 'A' && *p <= 'Z') &&
      !(*p >= 'a' && *p <= 'z'))
  {
    PDIP_ERR("The command name must begin with a letter or '_': %s\n", p);
    errno = EINVAL;
    return -1;
  }

  // Get the 1st word
  pdip_new_argv(p);
  p ++;
  while (*p &&
         (
          ('_' == *p)              ||
          (*p >= '0' && *p <= '9') ||
          (*p >= 'A' && *p <= 'Z') ||
          (*p >= 'a' && *p <= 'z')
         )
        )
  {
    p ++;
  } // End while

  // End of cmd line ?
  if (!(*p) || (PDIP_COMMENT == *p))
  {
    return 0;
  }

  // Make sure that it is a blank
  if (!isspace(*p))
  {
    PDIP_ERR("Unexpected char inside command name: %s\n", p);
    errno = EINVAL;
    return -1;
  }

  // Overwrite the blank char with a NUL
  *p = '\0';
  p ++;

  // Following words are the parameters

next_param:

  // Skip the blanks
  while (isspace(*p))
  {
    p ++;
  } // End while

  // End of cmd line ?
  if (!(*p) || (PDIP_COMMENT == *p))
  {
    return 0;
  }

  // If it is a string
  if ('"' == *p)
  {
    goto get_string;
  }

  // It is a word, get it
  pdip_new_argv(p);
  p ++;
  while (*p           &&
         !isspace(*p) &&
         (PDIP_COMMENT != *p))
  {
    p ++;
  } // End while

  // End of cmd line ?
  if (!(*p) || (PDIP_COMMENT == *p))
  {
    return 0;
  }

  // Make sure it is a blank
  if (!isspace(*p))
  {
    PDIP_ERR("Unexpected char inside parameter name: %s\n", p);
    errno = EINVAL;
    return -1;
  }

  // Overwrite the blank char with a NUL
  *p = '\0';
  p ++;

  goto next_param;

get_string:

  pdip_new_argv(p);
  pString = p;
  p ++;

  state_1: // Look for ending '"'
    switch(*p)
    {
      case '"' : p ++;
                 // End of cmd line ?
                 if (!(*p) || (PDIP_COMMENT == *p))
                 {
                   return 0;
                 }

                 if (!isspace(*p))
                 {
                   PDIP_ERR("Unexpected char '%c' behind string '%s'\n", *p, pString);
                   errno = EINVAL;
                   return -1;
                 }

                 *p = '\0';
                 p ++;
                 goto next_param;
      case '\\' : p ++;
                  goto state_2;
      case '\0' : PDIP_ERR("Unterminated string '%s'\n", pString);
                  errno = EINVAL;
                  return -1;
      default   : p ++;
                  goto state_1;
    } // End switch

  state_2: // Character inhibition
    switch(*p)
    {
      case '\0' : PDIP_ERR("Unterminated string '%s'\n", pString);
                  errno = EINVAL;
                  return -1;
      default : p ++;
                goto state_1;
    } // End switch

  return -1;
} // pdip_split_cmdline





#if 0

//----------------------------------------------------------------------------
// Name        : pdip_reset_sigchld
// Description : Unset the handlers for the signals
//----------------------------------------------------------------------------
static void pdip_reset_sigchld(void)
{
sighandler_t p;
int          err_sav;

  p = signal(SIGCHLD, SIG_DFL);
  if (SIG_ERR == p)
  {
    err_sav = errno;
    PDIP_ERR("Error '%m' (%d) on signal()\n", errno);
    errno = err_sav;
    return;
  }

} // pdip_reset_sigchld

#endif // 0


//----------------------------------------------------------------------------
// Name        : pdip_kill_chld
// Description : Terminate the child process
//----------------------------------------------------------------------------
static void pdip_kill_chld(void)
{
int status;

  // Unset SIGCHLD
  // RECTIFICATION: We don't unset the SIGCHLD signal otherwise all
  //                subsequent calls to waitpid() fail with ECHILD and
  //                the sub-process is detached to belong to init and
  //                will live sometimes before the cyclic cleanup of init
  //
  //pdip_reset_sigchld();

  // If the child process was not finished just before the reset of SIGCHLD
  if (!pdip_dead_prog)
  {
    kill(pdip_pid, SIGTERM);
    sleep(1);

    // The signal handler for SIGCHLD may be triggered if the child dies

    // If the sub-process didn't died ==> SIGKILL
    if (!pdip_dead_prog)
    {
      kill(pdip_pid, SIGKILL);
    }

    // The SIGCHLD signal handler must be triggered

    // Wait for the termination of the child (this may be done
    // by the SIGCHLD handler but we need to wait to make sure
    // that the sub-process will not become a zombie)
    (void)waitpid(-1, &status, 0);
  }
} // pdip_kill_chld


//----------------------------------------------------------------------------
// Name        : pdip_sig_tt
// Description : Signal handler for SIGTTIN/OU
//----------------------------------------------------------------------------
static void pdip_sig_tt(int sig)
{
  switch(sig)
  {
    case SIGTTOU : // Attempt to write to terminal while in background mode
    {
      pdip_background_out = 1;

      (void)signal(SIGTTOU, SIG_DFL);
    }
    break;

    case SIGTTIN : // Attempt to read from terminal while in background mode
    {
      pdip_background_in = 1;

      // This will make subsequent reads from terminal fail with EIO
      (void)signal(SIGTTIN, SIG_IGN);
    }
    break;

    default :
    {
      assert(0);
    }
  } // End switch
} // pdip_sig_tt


//----------------------------------------------------------------------------
// Name        : pdip_read
// Description : Read input data
// Return      : Number of read bytes if OK
//               -1, if error
//               -2, if background mode
//----------------------------------------------------------------------------
static int pdip_read(
                      int           fd,
                      char         *buf,
                      size_t        l
                    )
{
int rc;
int err_sav;

  do
  {
    rc = read(fd, buf, l);
    if (-1 == rc)
    {
      if (EINTR == errno)
      {
        continue;
      }

      // If launched in background and the input is the terminal, we will receive a
      // SIGTTIN. The signal handler will set pdip_background_in to 1
      if ((pdip_in == fd) && (pdip_background_in))
      {
        return -2;
      }

      err_sav = errno;
      //PDIP_ERR("Error '%m' (%d) on read(fd:%d, l:%"PRISIZE"u)\n", errno, fd, l);
      errno = err_sav;
      return -1;
    }
  } while (rc < 0);

  return rc;
} // pdip_read


//----------------------------------------------------------------------------
// Name        : pdip_read_to
// Description : Read input data with a timeout
// Return      : Number of read bytes if OK
//               -1, if error (errno set to ETIMEDOUT if timeout)
//----------------------------------------------------------------------------
int pdip_read_to(
               int           fd,
               char         *buf,
               size_t        l,
               unsigned int  timeout    // Timeout in seconds
               )
{
int             rc;
fd_set          fdset;
struct timeval  to;

one_more_time:

  // Make the list of supervised file descriptors
  FD_ZERO(&fdset);
  FD_SET(fd, &fdset);

  to.tv_sec  = (time_t)timeout;
  to.tv_usec = 0;

  rc = select(fd + 1, &fdset, NULL, NULL, &to);

  switch (rc)
  {
    case -1 : // Error
    {
      if (EINTR == errno)
      {
        goto one_more_time;
      }

      return -1;
    }
    break;

    case 0: // Timeout
    {
      PDIP_DBG(6, "Timeout %u seconds while reading %d\n", timeout, fd);
      errno = ETIMEDOUT;
      return -1;
    }
    break;

    default : // Incoming data
    {
      assert(FD_ISSET(fd, &fdset));

      return pdip_read(fd, buf, l);
    }
    break;
  } // End switch

  return rc;
} // pdip_read_to



//----------------------------------------------------------------------------
// Name        : pdip_dump_outstanding_data
// Description : Get latest data from program
//----------------------------------------------------------------------------
static void pdip_dump_outstanding_data(void)
{
int rc;

  PDIP_DBG(0, "Outstanding data from program are:\n");

  if (pdip_loutstanding)
  {
    PDIP_DUMP2(0, pdip_outstanding_buf, pdip_loutstanding);
  }

  if (pdip_pty >= 0)
  {
    do
    {
      rc = pdip_read_to(pdip_pty,
                        pdip_buf,
                        pdip_bufsz,
                        0);

      if (rc > 0)
      {
        PDIP_DUMP2(0, pdip_buf, (unsigned int)rc);
      }
    } while (rc > 0);
  }

} // pdip_dump_outstanding_data


//----------------------------------------------------------------------------
// Name        : pdip_sig_chld
// Description : Signal handler for death of child
//----------------------------------------------------------------------------
static void pdip_sig_chld(int sig)
{
pid_t pid;
int   status;

  assert(SIGCHLD == sig);

  // Get the status of the child
  pid = waitpid(-1, &status, WNOHANG);

  PDIP_DBG(6, "Received SIGCHLD signal from process %d\n", pid);

  // If error
  if (-1 == pid)
  {
    //PDIP_ERR("Error %d '%m' from waitpid()\n", errno);
    assert(ECHILD == errno);
    return;
  }

  // If it is an asynchronous program
  if (pdip_pid != pid)
  {
    if (WIFEXITED(status))
    {
      PDIP_DBG(2, "Sub process with pid %d exited with code %d\n", pid, WEXITSTATUS(status));
    }
    else
    {
      if (WIFSIGNALED(status))
      {
        PDIP_DBG(1, "Sub process with pid %d finished with signal %d%s\n", pid, WTERMSIG(status), (WCOREDUMP(status) ? " (core dumped)" : ""));
      }
      else
      {
        PDIP_DBG(1, "Sub process with pid %d finished in error\n", pid);
      }
    }

    return;
  }

  // We must have at least one child
  if (-1 != pdip_pid)
  {
    // When PDIP encounters en EOF on its standard input, it closes
    // the PTY. This triggers an EOF on the program's side which may
    // finish as well. So, the waitpid() in the main() function catches
    // the status of the dead program and we receive a SIGCHLD to trigger
    // this handler. But in this case, the status is no longer available
    // of course...
    if (pid != -1)
    {
      if (WIFEXITED(status))
      {
        pdip_exit_prog = WEXITSTATUS(status);

        // Debug message if normal exit or debug activated
        PDIP_DBG(1, "Sub process with pid %d exited with code %d\n", pid, pdip_exit_prog);

        // If error and debug not activated, force an error message
        if (pdip_exit_prog != 0)
	{
          if (0 == pdip_debug)
	  {
            PDIP_DBG(0, "Sub process with pid %d exited with code %d\n", pid, pdip_exit_prog);
	  }

          // If error, dump the outstanding data in case error messages from
          // the program are available
          //pdip_dump_outstanding_data();
        }
      }
      else
      {
        if (WIFSIGNALED(status))
	{
          PDIP_ERR("Sub process with pid %d finished with signal %d%s\n", pid, WTERMSIG(status), (WCOREDUMP(status) ? " (core dumped)" : ""));
	}
        else
	{
          PDIP_ERR("Sub process with pid %d finished in error\n", pid);
	}

        // Default error exit code
        pdip_exit_prog = 1;

        // Dump the outstanding data in case error messages from
        // the program are available
        //pdip_dump_outstanding_data();
      } // End if exited
    }
    else
    {
      assert(ECHILD == errno);
    }
  }
  else
  {
    // We failed inside the fork() ==> See comment above pdip_chld_failed_on_exec
    // definition
    PDIP_DBG(1, "Child %d finished in error\n", pid);
    pdip_chld_failed_on_exec = 1;

    // Default error exit code
    pdip_exit_prog = 1;

    return;
  }

  // Warn the father
  pdip_dead_prog = 1;

  // Make sure that only one child is reported
  pid = waitpid(-1, &status, WNOHANG);

  // We must not have any terminated child
  assert((0 == pid) || ((-1 == pid) && (ECHILD == errno)));
} // pdip_sig_chld



//----------------------------------------------------------------------------
// Name        : pdip_sig_alarm
// Description : Signal handler for ALARM
//----------------------------------------------------------------------------
static void pdip_sig_alarm(int sig)
{
  assert(SIGALRM == sig);

  PDIP_DBG(4, "Timeout while waiting for child\n");

  pdip_kill_chld();

  exit(1);
} // pdip_sig_alarm




//----------------------------------------------------------------------------
// Name        : pdip_capture_sigchld
// Description : Set the handlers for the SIGCHLD signal
//----------------------------------------------------------------------------
static void pdip_capture_sigchld(void)
{
struct sigaction    action;
sigset_t            sig_set;
int                 rc;
int                 err_sav;

  // We block SIGCHLD while managing SIGCHLD
  rc = sigemptyset(&sig_set);
  rc = sigaddset(&sig_set, SIGCHLD);

  // Set the handler for SIGCHLD
  memset(&action, 0, sizeof(action));
  action.sa_handler   = pdip_sig_chld;
  action.sa_mask      = sig_set;
  action.sa_flags     = SA_NOCLDSTOP | SA_RESTART | SA_RESETHAND;
  rc = sigaction(SIGCHLD, &action, NULL);
  if (0 != rc)
  {
    err_sav = errno;
    PDIP_ERR("Error '%m' (%d) on sigaction()\n", errno);
    errno = err_sav;
    return;
  }
} // pdip_capture_sigchld


//----------------------------------------------------------------------------
// Name        : pdip_sig_exception
// Description : Exception handler
//----------------------------------------------------------------------------
static void pdip_sig_exception(
                               int        sig,
                               siginfo_t *info,
                               void      *context
			      )
{
//ucontext_t  *pCtx = (ucontext_t *)context;

  (void)context;

/*
  The structure siginfo_t contains the following information :
    . The si_signo member contains the system-generated signal number
    . The si_errno member may contain implementation-defined additional
      error information; if non-zero, it contains an error number
      identifying the condition that caused the signal to be generated.
    . The si_code member contains a code identifying the cause of the signal.
      If the value of si_code is less than or equal to 0, then the signal was
      generated by a process and si_pid and si_uid,  respectively, indicate
      the process ID and the real user ID of the sender.
*/
  PDIP_ERR("PDIP crashed with signal %d at address %p\n", sig, info->si_addr);

  abort();
} // pdip_sig_exception


//----------------------------------------------------------------------------
// Name        : pdip_capture_exception_sig
// Description : Set the handlers for the exception signals
//----------------------------------------------------------------------------
static void pdip_capture_exception_sig(void)
{
struct sigaction    action;
sigset_t            sig_set;
int                 rc;
int                 err_sav;
int                 sigs[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, 0};
int                 i;

  rc = sigemptyset(&sig_set);
  i = 0;
  while (sigs[i])
  {
    rc = sigaddset(&sig_set, sigs[i]);
    i ++;
  }

  // Set the handler
  i = 0;
  while (sigs[i])
  {
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = pdip_sig_exception;
    action.sa_mask      = sig_set;
    action.sa_flags     = SA_NOCLDSTOP | SA_RESTART | SA_RESETHAND | SA_SIGINFO;
    rc = sigaction(sigs[i], &action, NULL);
    if (0 != rc)
    {
      err_sav = errno;
      PDIP_ERR("Error '%m' (%d) on sigaction()\n", errno);
      errno = err_sav;
      return;
    }

    i ++;
  } // End while

} // pdip_capture_exception_sig





//----------------------------------------------------------------------------
// Name        : pdip_read_line
// Description : Read input data from 'fd' until end of line or end of file
//               or when 'l' chars have been read
// Return      : Number of read bytes if OK
//               -1, if error
//               -2, if background mode
//----------------------------------------------------------------------------
static int pdip_read_line(
                      int           fd,
                      char         *buf,
                      size_t        l
                    )
{
int rc;
int offset = 0;
int err_sav;

  assert(l > 0);

  do
  {
    rc = pdip_read(fd, &(buf[offset]), 1);
    if (-1 == rc)
    {
      err_sav = errno;
      PDIP_ERR("Error '%m' (%d) on read(%d)\n", errno, fd);
      errno = err_sav;
      return -1;
    }

    // Background mode ?
    if (-2 == rc)
    {
      return -2;
    }

    // End of file ?
    if (0 == rc)
    {
      return offset;
    }

    // End of line ?
    if ('\n' == buf[offset])
    {
      return offset + 1;
    }

    // One more char...
    offset ++;

    // End of buffer ?
    if (offset >= (int)l)
    {
      return offset;
    }

  } while (1);

} // pdip_read_line




//----------------------------------------------------------------------------
// Name        : pdip_write
// Description : Write out data
// Return      : len, if OK
//               -1, if error (errno is set)
//               -2, if background mode
//----------------------------------------------------------------------------
static int pdip_write(
                      int           fd,
                      const char    *buf,
                      size_t         len
                     )
{
int    rc;
size_t l;
int    saved_errno;

  l = 0;
  do
  {
one_more_time:

    rc = write(fd, buf + l, len - l);

    if (rc < 0)
    {
      if (EINTR == errno)
      {
        goto one_more_time;
      }

      // If launched in background and the output is the terminal, we will receive a
      // SIGTTOU. The signal handler will set pdip_background_out to 1
      if ((pdip_out == fd || pdip_err == fd) && (pdip_background_out))
      {
        return -2;
      }

      saved_errno = errno;
      PDIP_ERR("Error '%m' (%d) on write()\n", errno);
      errno = saved_errno;
      return -1;
    }

    assert((l + (size_t)rc) <= len);
    l += (unsigned int)rc;
  } while (l != len);

  return (int)len;

} // pdip_write


//----------------------------------------------------------------------------
// Name        : pdip_flush
// Description : Flush out the outstanding data from the program
// Return      : None
//----------------------------------------------------------------------------
static void pdip_flush(int fd)
{
int  rc;

  do
  {
    rc = pdip_read_to(fd, pdip_buf, pdip_bufsz, 1);
    if (rc > 0)
    {
      (void)pdip_write(pdip_out, pdip_buf, rc);
    }
  } while (rc > 0);

} // pdip_flush


//----------------------------------------------------------------------------
// Name        : pdip_handle_synchro
// Description : Handle the synchronization string
// Notes       : . The lines are considered to be 256 chars long max
//               . If lines are more than 256 chars, they are considered as
//               multiple lines from a pattern matching point of view
//               . NUL chars are removed
//
// Return      : 0, if synchro string found
//               1, if synchro string not found
//              -1, if error
//----------------------------------------------------------------------------
static int pdip_handle_synchro(
                        regex_t       *regex,
                        char          *buffer,
                        unsigned int  *lbuf    // Remaining data in the buffer
                              )
{
int             rc;
regmatch_t      result;
static char    *line = NULL;
static size_t   line_sz = 0;
static char    *line_skip = NULL;
char           *eol;      // End of line
char           *eob;      // End of buffer
char           *p;        // Buffer pointer
unsigned int    i, l;
unsigned int    nb_skip;
int             eol_fnd;

  PDIP_DBG(2, "Looking for synchro string: <%s>\n", pdip_synchro);

  // The size of the working buffer must be at least the size of the input
  // buffer + 1 for the terminating NUL
  if (*lbuf >= line_sz)
  {
    l = line_sz + (*lbuf - line_sz) + 1;
    line = (char *)realloc(line, l);
    if (!line)
    {
      PDIP_ERR("Memory allocation error (%u bytes)\n", l);
      return -1;
    }

    // New size of the working buffer
    line_sz = l;

    line_skip = (char *)realloc(line_skip, line_sz);
    if (!line_skip)
    {
      PDIP_ERR("Memory allocation error (%"PRISIZE"u bytes)\n", line_sz);
      return -1;
    }
  } // End if working buffer to short

  while (*lbuf)
  {
    // End of buffer
    eob = buffer + *lbuf;

    p = eol = buffer;

    // Copy the line into a temporary buffer to ignore the NUL chars coming
    // from the program
    // (e.g. telnet sends lots of NUL chars during the login session)
    i       = 0;
    nb_skip = 0;
    eol_fnd = 0;
    while (eol < eob)
    {
      assert(i < (line_sz - 1));

      // We stop on 'end of line'
      if ('\n' == *eol)
      {
        line[i] = '\n';
        line_skip[i] = nb_skip;
        i ++;
        eol_fnd = 1;
        break;
      }
      // We skip control chars for pattern matching
      else if (isprint(*eol) || isblank(*eol))
      {
        line[i] = *eol;
        line_skip[i] = nb_skip;
        i ++;
      }
      else
      {
        // Count the number of skipped chars
        nb_skip ++;
      }

      eol ++;
    } // End while

    // Here, either *eol is '\n' or i == (line_sz - 1) or eol == eob
    // "i" is the number of written chars in line[]

    // In any case, we terminate the line to make the pattern matching work
    line[i] = '\0';

    PDIP_DBG(2, "Current line is:\n");
    PDIP_DUMP2(2, line, strlen(line) + 1);

    // Normally we are tempted to launch the pattern matching only if we have
    // a complete line because the pattern may concern the end of line
    // (i.e. '$'). But this would block the program which prints lines without
    // end of line (like login or password requests !)
    rc = regexec(regex, line, 1, &result, 0);
    if (0 == rc)
    {
      PDIP_DBG(4, "Pattern matching succeeded !\n");

      if (result.rm_eo > 0)
      {
        // Number of chars to output
        l = result.rm_eo + line_skip[result.rm_eo - 1];

        // Print out the line up to the end of the matching substring
        pdip_write(pdip_out, p, l);

        // Remaining data in the buffer
        *lbuf -= l;

        // Shift the buffer before returning
        // (don't forget the skipped control chars)
        memmove(buffer, p + l, *lbuf);
      }
      else
      {
        // If result.rm_eo == 0, this means that we are matching a beginning of line
        l = 0;
      }

      PDIP_DBG(2, "Synchronized on <%s> at offset %u\n==> Outstanding data are (lbuf = %u):\n", pdip_synchro, l, *lbuf);
      PDIP_DUMP2(2, buffer, *lbuf);

      return 0;
    }
    else
    {
      PDIP_DBG(4, "Pattern matching failed !\n");

      // Number of read chars in this line
      if (i)
      {
        l = i + line_skip[i-1];
      }
      else
      {
        l = nb_skip;
      }

      // Print out the entire line only if we have a complete line
      // otherwise we must wait for additional chars to complete
      // the pattern matching
      if (eol_fnd)
      {
        pdip_write(pdip_out, p, l);

        // New buffer size
        assert(*lbuf >= l);
        *lbuf -= l;

        p += l;
        eol = p;

        // Shift the buffer before returning
        // (don't forget the skipped control chars)
        memmove(buffer, p, *lbuf);

        PDIP_DBG(2, "Still not synchronized on <%s> at offset %u\n==> Outstanding data are (lbuf = %u):\n", pdip_synchro, l, *lbuf);
        PDIP_DUMP2(2, buffer, *lbuf);
      }
      else // No end of line
      {
        // We keep the data in the buffer waiting for additionnal data
        PDIP_DBG(2, "Still not synchronized on <%s> at offset %u\n==> Waiting for more data, outstanding data are (lbuf = %u):\n", pdip_synchro, l, *lbuf);
        PDIP_DUMP2(2, p, *lbuf);
        break;
      }
    } // End if pattern matching OK
  } // End while

  return 1;
} // pdip_handle_synchro


#define PDIP_CTRL(x)  ((char)((x) & ~0x40))


//----------------------------------------------------------------------------
// Name        : pdip_format_params
// Description : Translate a formatted string into a string
// Return      : 0, if OK
//               -1, if error
//----------------------------------------------------------------------------
static int pdip_format_params(
                              char         *params,   // Parameter string to format
                              char         *out,      // Result of the formatting
                              unsigned int *lout
                             )
{
char         *p;
unsigned int  i, j;

  if (!params || !out || !lout)
  {
    PDIP_ERR("NULL parameters\n");
    return -1;
  }

  // Translate the string
  p = params;
  i = 0;
  j = 0;

  if (p[i] != '"')
  {
    PDIP_ERR("The string parameter must begin with a double quote: '%s'\n", params);
    return -1;
  }

  i ++;

state_1: // Look for ending double quote

                if (!(p[i]))
                {
                  PDIP_ERR("The string parameter must end with a double quote: '%s'\n", params);
                  return -1;
                }

                if (j >= *lout)
                {
                  PDIP_ERR("The string parameter is too long (%u chars max)\n", *lout);
                  return -1;
                }

  switch(p[i])
  {
    case '"' : out[j++] = '\0';
               goto state_end;
    case '\\' : i ++;
                goto state_2;
    case '^' : i ++;
               goto state_3;
    default   : out[j++] = p[i++];
                goto state_1;
  }  // End switch

state_2: // Handle escape char
                if (!(p[i]))
                {
                  PDIP_ERR("The string parameter must end with a double quote: '%s'\n", params);
                  return -1;
                }

                if (j >= *lout)
                {
                  PDIP_ERR("The string parameter is too long (%u chars max)\n", *lout);
                  return -1;
                }

  switch(p[i])
  {
    case '\\' : out[j++] = '\\';
                i ++;
                goto state_1;
    case 'n'  : out[j++] = '\n';
                i ++;
                goto state_1;
    case 't'  : out[j++] = '\t';
                i ++;
                goto state_1;
    case 'b'  : out[j++] = '\b';
                 i ++;
                goto state_1;
    case 'a'  : out[j++] = '\a';
                i ++;
                goto state_1;
    case '"'  : out[j++] = '"';
                i ++;
                goto state_1;
    case 'f'  : out[j++] = '\f';
                i ++;
                goto state_1;
    case 'r'  : out[j++] = '\r';
                i ++;
                goto state_1;
    case 'v'  : out[j++] = '\v';
                i ++;
                goto state_1;
    case '['  : out[j++] = 0x1b;
                i ++;
                goto state_1;
    case ']'  : out[j++] = 0x1d;
                i ++;
                goto state_1;
    case '^'  : out[j++] = '^';
                i ++;
                goto state_1;
    default   : out[j++] = p[i++];
                goto state_1;
  }  // End switch


state_3: // Handle control char
                if (!(p[i]))
                {
                  PDIP_ERR("The string parameter must end with a double quote: '%s'\n", params);
                  return -1;
                }

                if (j >= *lout)
                {
                  PDIP_ERR("The string parameter is too long (%u chars max)\n", *lout);
                  return -1;
                }

  out[j++] = PDIP_CTRL(p[i]);
  i ++;
  goto state_1;

state_end:

  assert(j > 0);
  *lout = j - 1;

  return 0;
} // pdip_format_params





//----------------------------------------------------------------------------
// Name        : pdip_compile_regex
// Description : Compile a regular expression
//----------------------------------------------------------------------------
static void pdip_compile_regex
                      (
                       char    *pattern,  // Pattern to compile
                       regex_t *regex     // Compiled regular expression
                      )
{
char *p;
int   rc;

  assert(pattern && pattern[0] && regex);

  strncpy(pdip_synchro, pattern, sizeof(pdip_synchro) - 1);
  pdip_synchro[sizeof(pdip_synchro) - 1] = '\0';

  // Look for the starting double quote
  p = pattern;
  if (*p != '"')
  {
    PDIP_ERR("The string parameter must begin with a double quote\n");
    return;
  }

  // Look for the terminating double quote tolerating the terminating blanks
  if (!(*(p+1)))
  {
    PDIP_ERR("The string parameter must end with a double quote\n");
    return;
  }
  p += strlen(pattern) - 1;
  while (isspace(*p))
  {
    p --;
  } // End while

  if ((p == pattern) || (*p != '"'))
  {
    PDIP_ERR("The string parameter must end with a double quote\n");
    return;
  }

  // Suppress the starting and terminating double quotes
  pattern ++;
  *p = '\0';

  // Compile the regular expression
  //
  // . After compilation, the compiler returns the number of parenthesized
  //   subexpressions in regex->re_nsub
  //
  memset(regex, 0, sizeof(regex_t));
  PDIP_DBG(3, "Compiling <%s>\n", pattern);
  rc = regcomp(regex, pattern, REG_EXTENDED|REG_NEWLINE);
  if (0 != rc)
  {
    PDIP_ERR("Bad regular expression <%s>\n", pattern);
    return;
  }

  PDIP_DBG(3, "Number of sub expressions in regex (%s): %"PRISIZE"u\n", pattern, regex->re_nsub);

  return;
} // pdip_compile_regex


//----------------------------------------------------------------------------
// Name        : pdip_terminal
// Description : Simulate a simple terminal in front of the program
// Return      : 0, if end of input file
//               -1, error
//----------------------------------------------------------------------------
static int pdip_terminal(void)
{
int             rc;
fd_set          fdset;
int             max_fd;
int             fd_input = pdip_in;           // To get data from user
int             fd_program = pdip_pty;        // To interact with the program
int             loop = 1;
pid_t           pid = pdip_pid;
int             err_sav;
unsigned int    lbuf;

  while (loop)
  {
    // If the program is dead
    if (pdip_dead_prog)
    {
      PDIP_DBG(1, "Command (pid = %u) is finished\n", pid);

      // This is not an error ?
      rc = 0;
      goto end;
    } // End if program dead

    // Set the file descriptors to listen to
    FD_ZERO(&fdset);
    FD_SET(fd_input, &fdset);
    FD_SET(fd_program, &fdset);

    max_fd = (fd_input > fd_program ? fd_input + 1 : fd_program + 1);

    rc = select(max_fd, &fdset, NULL, NULL, NULL);

    switch(rc)
    {
      case -1 :
      {
        if (EINTR == errno)
        {
          continue;
        }

        err_sav = errno;
        PDIP_ERR("Error '%m' (%d) on select\n", errno);
        errno = err_sav;
        rc = -1;
        goto end;
      }
      break;

      default :
      {
        // If data from program
        if (FD_ISSET(fd_program, &fdset))
        {
          rc = pdip_read(fd_program,
                         pdip_buf,
                         pdip_bufsz - 1);
          if (rc < 0)
          {
            rc = -1;
            goto end;
	  }

          if (0 == rc)
          {
            PDIP_ERR("End of program\n");
            goto end;
	  }

          pdip_buf[rc] = '\0';

          // Length of the buffer
          lbuf = (unsigned int)rc;

          PDIP_DBG(2, "Received %u bytes from program:\n", lbuf);
          PDIP_DUMP2(2, pdip_buf, lbuf);

          // Forward the data to the output
          rc = pdip_write(pdip_out, pdip_buf, lbuf);
          if ((unsigned int)rc != lbuf)
          {
            if (-2 != rc)
	    {
              PDIP_ERR("Error on write\n");
              rc = -1;
              goto end;
	    }
          }
        } // End if data from program

        // If data from user
        if (FD_ISSET(fd_input, &fdset))
        {
          rc = pdip_read_line(fd_input, pdip_buf, pdip_bufsz - 1);
          if (-1 == rc)
	  {
            rc = -1;
            goto end;
	  }

          // EOF
          if (0 == rc)
          {
            goto end;
	  }

          // If background mode
          if (-2 == rc)
	  {
            // Simulate no input data
            rc = 0;
	  }

          rc = pdip_write(pdip_pty, pdip_buf, (unsigned int)rc);
          if (rc < 0)
	  {
            rc = -1;
            goto end;
	  }
        } // End if data from user
      }
      break;
    } // End switch
  } // End while

end:

  return rc;
} // pdip_terminal


typedef struct
{
  const char *name;
  int         sig;
} pdip_sig2name;

static pdip_sig2name pdip_signals[] =
{
  { "HUP",  SIGHUP  },
  { "INT",  SIGINT  },
  { "QUIT", SIGQUIT },
  { "ILL",  SIGILL  },
  { "TRAP", SIGTRAP },
  { "ABRT", SIGABRT },
  { "BUS",  SIGBUS  },
  { "FPE",  SIGFPE  },
  { "KILL", SIGKILL },
  { "USR1", SIGUSR1 },
  { "SEGV", SIGSEGV },
  { "USR2", SIGUSR2 },
  { "PIPE", SIGPIPE },
  { "ALRM", SIGALRM },
  { "TERM", SIGTERM },

  { NULL, 0 },
};


//----------------------------------------------------------------------------
// Name        : pdip_pdip_sigstr2num
// Description : Translate a signal name into its numerical value
// Return      : signal value, if OK
//               -1, if error
//----------------------------------------------------------------------------
static int pdip_sigstr2num(const char *sig)
{
unsigned int i;

  i = 0;
  while (pdip_signals[i].name)
  {
    if (!strcmp(pdip_signals[i].name, sig))
    {
      return pdip_signals[i].sig;
    }

    i ++;
  } // End while

  return -1;
} // pdip_sigstr2num


//----------------------------------------------------------------------------
// Name        : pdip_interact
// Description : Interaction with user and program
// Return      : 0, if end of input file
//               -1, error
//               -2, timeout
//               -3, syntax error
//----------------------------------------------------------------------------
static int pdip_interact(void)
{
int             rc;
struct timeval  timeout, *pTimeout = NULL;
unsigned int    to = 0;
char           *pKeyword = NULL;
fd_set          fdset;
int             max_fd;
int             fd_input = pdip_in;     // To get data from user
int             fd_program = -1;        // To interact with the program
unsigned int    lbuf, l;
char           *p;
unsigned int    lineno = 0;
int             loop = 1;
pid_t           pid = pdip_pid;
regex_t         regex;
int             synchro = 0;
int             err_sav;

  // In case we "goto end" before any allocation of regular expressions
  memset(&regex, 0, sizeof(regex));

  while (loop)
  {
    // If the program is dead
    if (pdip_dead_prog)
    {
      PDIP_DBG(1, "Command (pid = %u) is finished\n", pid);

      // Print out the outstanding data received from the program
      // Later, I will launch a pattern matching with the outstanding data
      // if a synchro is on track because we may want to synchronize on the
      // latest data printed out by the program.
      PDIP_DBG(5, "Flushing outstanding data...\n");
      if (pdip_loutstanding)
      {
        pdip_write(pdip_out, pdip_outstanding_buf, pdip_loutstanding);
      }
      if (pdip_pty >= 0)
      {
        pdip_flush(pdip_pty);
      }

      // This is not an error ?
      rc = 0;
      goto end;
    } // End if program is dead

    // Set the file descriptors to listen to
    FD_ZERO(&fdset);
    if (fd_input >= 0)
    {
      FD_SET(fd_input, &fdset);
    }
    if (fd_program >= 0)
    {
      FD_SET(fd_program, &fdset);
    }
    max_fd = (fd_input > fd_program ? fd_input + 1 : fd_program + 1);

    rc = select(max_fd, &fdset, NULL, NULL, pTimeout);

    switch(rc)
    {
      case -1 :
      {
        if (EINTR == errno)
        {
          continue;
        }

        err_sav = errno;
        PDIP_ERR("Error '%m' (%d) on select\n", errno);
        errno = err_sav;
        rc = -1;
        goto end;
      }
      break;

      case 0 : // Timeout
      {
        PDIP_ERR("Line %u: Timeout (%u seconds)\n", lineno, to);
        pdip_dump_outstanding_data();
        errno = ETIMEDOUT;
        rc = -2;
        goto end;
      }
      break;

      default :
      {
        // If data from program
        if (fd_program >= 0)
        {
          if (FD_ISSET(fd_program, &fdset))
          {
            rc = pdip_read(fd_program,
                           pdip_buf,
                           pdip_bufsz - 1);
            if (rc < 0)
	    {
              rc = -1;
              goto end;
	    }

            if (0 == rc)
	    {
              PDIP_ERR("End of program\n");
              goto end;
	    }

            pdip_buf[rc] = '\0';

            // Length of the buffer
            lbuf = (unsigned int)rc;

            PDIP_DBG(2, "Received %u bytes from program:\n", lbuf);
            PDIP_DUMP2(2, pdip_buf, lbuf);

            // Append the read data to the outstanding buffer
            if (lbuf > (pdip_outstanding_buf_sz - pdip_loutstanding))
	    {
              // Not enough room ==> Increase the buffer size
              pdip_outstanding_buf = (char *)realloc(pdip_outstanding_buf, pdip_outstanding_buf_sz + lbuf);
              if (!pdip_outstanding_buf)
	      {
                PDIP_ERR("Not enough memory for outstanding data (max = %u, current size = %u, new input data sz = %u)\n", pdip_outstanding_buf_sz, pdip_loutstanding, lbuf);
                rc = -1;
                goto end;
	      }

              pdip_outstanding_buf_sz += lbuf;
            } // End if enough room

            memcpy(pdip_outstanding_buf + pdip_loutstanding, pdip_buf, lbuf);
            pdip_loutstanding += lbuf;

            PDIP_DBG(2, "Outstanding data (%u) are:\n", pdip_loutstanding);
            PDIP_DUMP2(2, pdip_outstanding_buf, pdip_loutstanding);

            // Is there a synchro ?
            if (synchro)
	    {
              rc = pdip_handle_synchro(&regex, pdip_outstanding_buf, &pdip_loutstanding);
              if (0 == rc)
              {
                fd_input   = pdip_in;
                synchro    = 0;

		if (!pdip_backread)
		{
                  fd_program = -1;
		}
              }
              else
	      {
                if (-1 == rc)
                {
                  PDIP_ERR("Error\n");
                  rc = -1;
                  goto end;
                }
	      }
	    }
            else // There is no synchro
            {
              PDIP_DBG(3, "No active synchro ==> Input data is stored\n");
            }
          } // End if data from program
        } // End if read program activated

        // If we are allowed to read from user (i.e. no synchro)
        if (fd_input >= 0)
        {
          // If there are data from user
          if (FD_ISSET(fd_input, &fdset))
	  {
            rc = pdip_read_line(fd_input, pdip_buf, pdip_bufsz - 1);
            if (-1 == rc)
	    {
              rc = -1;
              goto end;
	    }

            // EOF
            if (0 == rc)
	    {
              // Print out the outstanding data received from the program
              if (pdip_loutstanding)
              {
                (void)pdip_write(pdip_out, pdip_outstanding_buf, pdip_loutstanding);
              }

              goto end;
	    }

            // If background mode
            if (-2 == rc)
	    {
              // Simulate no input data
              rc = 0;
	    }

            // Make a string
            pdip_buf[rc] = '\0';

            // Increment the number of input lines
            lineno ++;

            // Make the input buffer become a string (overwrite the ending
            // '\n' char)
            p = pdip_buf;
            while (*p && ('\n' != *p))
	    {
              p ++;
	    } // End while

            // Terminate the string
            *p = '\0';

            // Split the command line into parameters
            pdip_reset_argv();
            rc = pdip_split_cmdline(pdip_buf);
            if (rc != 0)
            {
              PDIP_ERR("Line %u: Syntax error\n", lineno);
              rc = -3;
              errno = EINVAL;
              goto end;
            }

            // Empty line ?
            if (!pdip_argc)
	    {
              continue;
	    }

            // Get the keyword
            pKeyword = pdip_argv[0];

            // Timeout ?
            if (!strcmp("timeout", pKeyword))
	    {
              if (pdip_argc != 2)
	      {
                PDIP_ERR("Line %u: A parameter is required for 'timeout'\n", lineno);
	      }

              to = (unsigned int)atoi(pdip_argv[1]);

              PDIP_DBG(1, "Input line %u: %s %u\n", lineno, pKeyword, to);
	    } // End if timeout

            // recv ?
            else if (!strcmp("recv", pKeyword))
	    {
              // Free the current regular expression if any
              regfree(&regex);

              if (1 == pdip_argc)
	      {
                // With no parameters, we match an end of line
                // ==> Copy the pattern in a writable buffer
                strncat(pKeyword, " \"$\"", pdip_bufsz - (unsigned int)(p - pdip_buf));
                *p = '\0';
                PDIP_DBG(1, "Input line %u: %s %s\n", lineno, pKeyword, p+1);
                pdip_compile_regex(p + 1, &regex);
	      }
              else
              {
                if (2 == pdip_argc)
                {
                  PDIP_DBG(1, "Input line %u: %s %s\n", lineno, pKeyword, pdip_argv[1]);
                  pdip_compile_regex(pdip_argv[1], &regex);
                }
                else
                {
                  rc = -1;
                }
              }

              if (rc < 0)
              {
                PDIP_ERR("Line %u: Bad parameter\n", lineno);
                rc = -1;
                goto end;
              }


              fd_input = -1;

              fd_program = pdip_pty;

              synchro = 1;

              // Look for the synchro in the outstanding buffer if any
              // otherwise we may wait indefinitely a new input string
              // from the program if the synchro string is outstanding
              if (pdip_loutstanding)
              {
                rc = pdip_handle_synchro(&regex, pdip_outstanding_buf, &pdip_loutstanding);
                if (rc == 0)
                {
                  fd_input      = pdip_in;
		  if (!pdip_backread)
		  {
                    fd_program = -1;
  		  }
                  synchro       = 0;
                }
              }
	    } // End if recv

            // send ?
            else if (!strcmp("send", pKeyword))
	    {
              if (1 == pdip_argc)
	      {
                PDIP_DBG(1, "Input line %u: %s\n", lineno, pKeyword);
                // No parameter ==> Send a carriage return to the program
                pdip_write(pdip_pty, "\n", 1);
              }
              else
	      {
                if (2 == pdip_argc)
                {
                  PDIP_DBG(1, "Input line %u: %s %s\n", lineno, pKeyword, pdip_argv[1]);
                  l = pdip_bufsz;
                  // 'params' and 'pdip_buf' are overlapping, so we use pdip_buf1 to avoid any future pb
                  rc = pdip_format_params(pdip_argv[1], pdip_buf1, &l);
                  if (rc < 0)
                  {
                    PDIP_ERR("Line %u: Bad parameters\n", lineno);
                    rc = -1;
                    goto end;
                  }
                  if (l)
                  {
                    rc = pdip_write(pdip_pty, pdip_buf1, l);
                  }
                  else
                  {
                    // Empty string ==> Send a carriage return to the program
                    rc = pdip_write(pdip_pty, "\n", 1);
                  }

                  if (rc < 0)
		  {
                    rc = -1;
                    goto end;
		  }
	        }
                else
                {
                  PDIP_ERR("Line %u: Bad parameters\n", lineno);
                  rc = -1;
                  goto end;
                }
              }
	    } // End if send

            // sig ?
            else if (!strcmp("sig", pKeyword))
	    {
              if (2 == pdip_argc)
              {
	      int sig = pdip_sigstr2num(pdip_argv[1]);

	        if (sig < 0)
		{
                  PDIP_ERR("Line %u: Bad signal name '%s'\n", lineno, pdip_argv[1]);
                  rc = -1;
                  goto end;
		}
                PDIP_DBG(1, "Input line %u: %s %s\n", lineno, pKeyword, pdip_argv[1]);

                rc = kill(pid, sig);
                if (rc < 0)
		{
                  PDIP_ERR("Line %u: Error %d while sending signal '%s'\n", lineno, errno, pdip_argv[1]);
                  goto end;
		}

                rc = 0;
              }
	    } // End if send

            // sleep ?
            else if (!strcmp("sleep", pKeyword))
	    {
            unsigned int duration;

              if (2 != pdip_argc)
	      {
                PDIP_ERR("Line %u: A parameter is required for 'sleep'\n", lineno);
                rc = -1;
                goto end;
	      }
              PDIP_DBG(1, "Input line %u: %s %s\n", lineno, pKeyword, pdip_argv[1]);

              duration = (unsigned int)atoi(pdip_argv[1]);
              sleep(duration);
	    } // End if sleep

            // print ?
            else if (!strcmp("print", pKeyword))
	    {
              if (2 != pdip_argc)
	      {
                PDIP_ERR("Line %u: A parameter is required for 'print'\n", lineno);
                rc = -1;
                goto end;
	      }

              // Translate the format into a string
              l = pdip_bufsz;
              rc = pdip_format_params(pdip_argv[1], pdip_buf1, &l);
              if (rc < 0)
              {
                PDIP_ERR("Line %u: Bad parameters\n", lineno);
                rc = -1;
                goto end;
              }
              PDIP_DBG(1, "Input line %u: %s %s\n", lineno, pKeyword, pdip_argv[1]);
              pdip_write(pdip_out, pdip_buf1, l);
	    } // End if print

            // dbg ?
            else if (!strcmp("dbg", pKeyword))
	    {
	    int old_dbg = pdip_debug;

              if (2 != pdip_argc)
	      {
                PDIP_ERR("Line %u: A parameter is required for 'dbg'\n", lineno);
                rc = -1;
                goto end;
	      }

              // Display the trace before setting the new debug mode if the debug mode is set
              PDIP_DBG(1, "Input line %u: %s %d to %s\n", lineno, pKeyword, pdip_debug, pdip_argv[1]);

              pdip_debug = atoi(pdip_argv[1]);

              // Display the trace after setting the new debug mode if the previous debug mode was unset
              if (!old_dbg)
	      {
                PDIP_DBG(1, "Input line %u: %s %u to %u\n", lineno, pKeyword, old_dbg, pdip_debug);
	      }
	    } // End if dbg

            // exit ?
            else if (!strcmp("exit", pKeyword))
	    {
              PDIP_DBG(1, "Input line %u: %s\n", lineno, pKeyword);
              loop = 0;
	    } // End if exit

            // sh ?
            else if (!strcmp("sh", pKeyword))
	    {
	    int   synchronous_sh;
            int   status;
            pid_t pid_sh;

              PDIP_DBG(1, "Input line %u: %s\n", lineno, pKeyword);

              if (pdip_argc < 2)
	      {
                PDIP_ERR("Line %u: Parameters are required for 'sh'\n", lineno);
                rc = -1;
                goto end;
	      }

              if (!strcmp("-s", pdip_argv[1]))
	      {
                if (pdip_argc < 3)
	        {
                  PDIP_ERR("Line %u: Parameters are required for 'sh'\n", lineno);
                  rc = -1;
                  goto end;
		}

                synchronous_sh = 1;

                PDIP_DBG(1, "Input line %u: %s %s %s...\n", lineno, pKeyword, pdip_argv[1], pdip_argv[2]);

                // Reset signals
                (void)signal(SIGCHLD, SIG_DFL);

  	        pid_sh = fork();
	      }
              else
	      {
                synchronous_sh = 0;

                PDIP_DBG(1, "Input line %u: %s %s...\n", lineno, pKeyword, pdip_argv[1]);

  	        pid_sh = fork();
	      }

              switch(pid_sh)
	      {
	        case -1:
		{
                  PDIP_ERR("Line %u: fork() failed with error %d ('%m')\n", lineno, errno);
                  rc = -1;
                  goto end;
		}
                break;

	        case 0 : // Child
		{
		char **av;
                int    sz;
                int    i;

                  // Execute the program
                  if (synchronous_sh)
		  {
                    sz = pdip_argc - 1;
		    av = (char **)malloc(sz * sizeof(char *));
                    if (av)
		    {
                      for (i = 0; i < (sz - 1); i ++)
		      {
                        av[i] = pdip_argv[i + 2];
		      } // End for
                      av[i] = NULL;
                      (void)execvp(pdip_argv[2], av);
		    }
                    else
		    {
                      PDIP_ERR("Line %u: malloc(%"PRISIZE"u) failed with error %d ('%m')\n"
                               , lineno, (size_t)(pdip_argc * sizeof(char *)), errno);
		    }
		  }
                  else // Asynchronous mode
		  {
                    sz = pdip_argc;
		    av = (char **)malloc(sz * sizeof(char *));
                    if (av)
		    {
                      for (i = 0; i < (sz - 1); i ++)
		      {
                        av[i] = pdip_argv[i + 1];
		      } // End for
                      av[i] = NULL;
                      (void)execvp(pdip_argv[1], av);
		    }
                    else
		    {
                      PDIP_ERR("Line %u: malloc(%"PRISIZE"u) failed with error %d ('%m')\n"
                               , lineno, (size_t)(pdip_argc * sizeof(char *)), errno);
		    }
		  } // End if synchronous

                  _exit(1);
		}
                break;

	        default : // Father
		{
                  if (synchronous_sh)
		  {
                    PDIP_DBG(2, "Waiting for end of process %d\n", pid_sh);

                    // Wait for the end of the child
                    if (waitpid(pid_sh, &status, 0) < 0)
		    {
                      PDIP_ERR("Line %u: waitpid(%d) failed with error %d ('%m')\n", lineno, pid_sh, errno);
                      rc = -1;
                      goto end;
		    }

                    // Restore the handler for SIGCHLD
                    pdip_capture_sigchld();

                    if (WIFEXITED(status))
		    {
                      PDIP_DBG(2, "%s %s %s... terminated with exit code %d\n"
                               ,
                               pKeyword, pdip_argv[1], pdip_argv[2], WEXITSTATUS(status));
		    }
                    else
		    {
                      PDIP_ERR("Line %u: Shell program failed\n", lineno);
                      rc = -1;
                      goto end;
		    }
		  } // End if synchronous

		}
                break;
	      } // End switch

	    } // End if sh

            // ?????
            else
	    {
              PDIP_ERR("Line %u: Unknown keyword '%s'\n", lineno, pKeyword);
              rc = -1;
              goto end;
	    }
	  } // End if data from user
        } // End if we are allowed to read from user

        PDIP_DBG(3, "Synchro %s, timeout %d seconds\n", (synchro ? "ON" : "OFF"), to);

        // If we are synchronizing (i.e. recv on track), the timeout is
        // reinitialized
        if (synchro && to)
        {
          timeout.tv_sec = (time_t)to;
          timeout.tv_usec = 0;
          pTimeout = &timeout;
        }
        else
        {
          pTimeout = NULL;
        }
      }
      break;
    } // End switch
  } // End while

end:
  regfree(&regex);

  PDIP_DBG(4, "End of interaction, rc = %d\n", rc);

  return rc;
} // pdip_interact




// ----------------------------------------------------------------------------
// Name   : main
// Usage  : Entry point
// ----------------------------------------------------------------------------
int main(
         int   ac,
         char *av[]
        )
{
unsigned int   options;
int            opt;
int            fds;
char          *pty_slave_name;
unsigned int   nb_cmds;
char         **params;
unsigned int   i;
int            rc;
char          *pScript = NULL;
int            err_sav;

  // Outputs of PDIP
  pdip_out = 1;
  pdip_err = 2;

  // Capture the exception signals
  pdip_capture_exception_sig();

  // Manage SIGTTOU/TTIN (for background mode)
  (void)signal(SIGTTOU, pdip_sig_tt);
  (void)signal(SIGTTIN, pdip_sig_tt);

  options = 0;

  while ((opt = getopt_long(ac, av, "s:b:d:VetopRh", pdip_longopts, NULL)) != EOF)
  {
    switch(opt)
    {
      case 's' : // Script to interpret
      {
        pScript = optarg;
        options |= 0x1;
      }
      break;

      case 'b' : // Set the size of the internal I/O buffer
      {
        pdip_bufsz = (unsigned int)atoi(optarg);
        options |= 0x2;
      }
      break;

      case 'd' : // Debug level
      {
        pdip_debug = atoi(optarg);
        options |= 0x4;
      }
      break;

      case 'V' : // Version
      {
        options |= 0x8;
      }
      break;

      case 'e' : // Redirect standard error as well
      {
        options |= 0x10;
      }
      break;

      case 't' : // Terminal behaviour
      {
        options |= 0x20;
      }
      break;

      case 'o' : // Dump outstanding data at the end of the session
      {
        options |= 0x40;
      }
      break;

      case 'p' : // Propagate exit code of controlled program
      {
        options |= 0x80;
      }
      break;

      case 'R' : // Read incoming data in background
      {
        options |= 0x100;
        pdip_backread = 1;
      }
      break;

      case 'h' : // Help
      {
        pdip_help(av[0]);
        exit(0);
      }
      break;

      case '?' :
      default :
      {
        pdip_help(av[0]);
        exit(1);
      }
    } // End switch
  } // End while

  // If version requested
  if (options & 0x8)
  {
    printf("PDIP version: %s\n", pdip_version);
    exit(0);
  }

  // If no command passed
  if (ac == optind)
  {
    PDIP_ERR("A command to launch is expected at the end of the command line \n\n");
    pdip_help(av[0]);
    exit(1);
  }

  // If the buffer size has not been set or is invalid
  if (pdip_bufsz <= 100)
  {
    pdip_bufsz = PDIP_DEF_BUFSZ;
  }

  // Get a master pty
  //
  // posix_openpt() opens a pseudo-terminal master and returns its file
  // descriptor.
  // It is equivalent to open("/dev/ptmx",O_RDWR|O_NOCTTY) on Linux systems :
  //
  //       . O_RDWR Open the device for both reading and writing
  //       . O_NOCTTY Do not make this device the controlling terminal for the process
  pdip_pty = posix_openpt(O_RDWR |O_NOCTTY);
  if (pdip_pty < 0)
  {
    err_sav = errno;
    PDIP_ERR("Impossible to get a master pseudo-terminal - errno = '%m' (%d)\n", errno);
    errno = err_sav;
    return 1;
  }

  // Grant access to the slave pseudo-terminal
  // (Chown the slave to the calling user)
  if (0 != grantpt(pdip_pty))
  {
    err_sav = errno;
    PDIP_ERR("Impossible to grant access to slave pseudo-terminal - errno = '%m' (%d)\n", errno);
    close(pdip_pty);
    errno = err_sav;
    return 1;
  }

  // Unlock pseudo-terminal master/slave pair
  // (Release an internal lock so the slave can be opened)
  if (0 != unlockpt(pdip_pty))
  {
    err_sav = errno;
    PDIP_ERR("Impossible to unlock pseudo-terminal master/slave pair - errno = '%m' (%d)\n", errno);
    close(pdip_pty);
    errno = err_sav;
    return 1;
  }

  // Get the name of the slave pty
  pty_slave_name = ptsname(pdip_pty);
  if (NULL == pty_slave_name)
  {
    err_sav = errno;
    PDIP_ERR("Impossible to get the name of the slave pseudo-terminal - errno = '%m' (%d)\n", errno);
    close(pdip_pty);
    errno = err_sav;
    return 1;
  }

  // Open the slave part of the terminal
  fds = open(pty_slave_name, O_RDWR);
  if (fds < 0)
  {
    err_sav = errno;
    PDIP_ERR("Impossible to open the slave pseudo-terminal - errno = '%m' (%d)\n", errno);
    close(pdip_pty);
    errno = err_sav;
    return 1;
  }

  // Allocate the parameters for the command
  nb_cmds = (unsigned int)(ac - optind);
  params = (char **)malloc(sizeof(char *) * (nb_cmds + 1));
  if (!params)
  {
    err_sav = errno;
    PDIP_ERR("Unable to allocate the parameters for the command (%u)\n", nb_cmds);
    errno = err_sav;
    return  1;
  }

  // Set the parameters for the command
  for (i = 0; i < nb_cmds; i ++)
  {
    params[i] = av[optind + (int)i];
  } // End for
  params[i] = NULL;

  // Capture SIGCHLD
  pdip_capture_sigchld();

  // Fork a child
  pdip_pid = fork();
  switch(pdip_pid)
  {
    case -1 :
    {
      err_sav = errno;
      PDIP_ERR("Error '%m' (%d) on fork()\n", errno);
      errno = err_sav;
      return 1;
    }
    break;

    case 0 : // Child
    {
    pid_t mypid = getpid();
    int   fd;

      assert(fds > 2);
      assert(pdip_pty > 2);

      // Redirect input/outputs to the slave side of PTY
      close(0);
      close(1);
      fd = dup(fds);
      assert(fd >= 0);
      fd = dup(fds);
      assert(fd >= 0);
      if (options & 0x10)
      {
        close(2);
        fd = dup(fds);
        assert(fd >= 0);
      }

      // Make some cleanups
      close(fds);
      close(pdip_pty);

      fds = 0;

#if 0
      // Remove controlling terminal if any
      rc = ioctl(fds, TIOCNOTTY);
      if (rc < 0)
      {
        err_sav = errno;
        PDIP_ERR("Error '%m' (%d) on ioctl(TIOCNOTTY)\n", errno);
        errno = err_sav;
        exit(1);
      }
#endif // 0

      // Make the child become a process session leader
      rc = setsid();
      if (rc < 0)
      {
        err_sav = errno;
        PDIP_ERR("Error '%m' (%d) on setsid()\n", errno);
        errno = err_sav;
        exit(1);
      }

      // As the child is a session leader, set the controlling terminal to be the slave side of the PTY
      rc = ioctl(fds, TIOCSCTTY, 1);
      if (rc < 0)
      {
        err_sav = errno;
        PDIP_ERR("Error '%m' (%d) on ioctl(TIOCSCTTY)\n", errno);
        errno = err_sav;
        exit(1);
      }

#if 0
      // Make the foreground process group on the terminal be the process id of the child
      rc = ioctl(fds, TIOCSPGRP, &mypid);
      if (rc < 0)
      {
        err_sav = errno;
        PDIP_ERR("Error '%m' (%d) on ioctl(TIOCSPGRP)\n", errno);
        errno = err_sav;
        exit(1);
      }
#endif // 0

      // Make the foreground process group on the terminal be the process id of the child
      rc = tcsetpgrp(fds, mypid);
      if (rc < 0)
      {
        err_sav = errno;
        PDIP_ERR("Error '%m' (%d) on setpgid()\n", errno);
        errno = err_sav;
        exit(1);
      }

      // Exec the program
      rc = execvp(params[0], params);

      // The error message can't be generated as the outputs are redirected to the PTY
      //PDIP_ERR("Error '%m' (%d) while running '%s'\n", errno, params[0]);

      _exit(1);
    }
    break;

    default: // Father
    {
    int   status;

      PDIP_DBG(1, "Forked process %d for program '%s'\n", pdip_pid, params[0]);

      // See comment above pdip_chld_failed_on_exec definition
      if (pdip_chld_failed_on_exec)
      {
        PDIP_ERR("Error while running '%s'\n", params[0]);
        return 1;
      }

      // The program is running
      pdip_dead_prog = 0;

      // Close the slave side of the PTY
      close(fds);

      // Allocate the I/O buffers
      pdip_buf = (char *)malloc(pdip_bufsz);
      assert(pdip_buf);
      pdip_buf1 = (char *)malloc(pdip_bufsz);
      assert(pdip_buf1);

      // Allocate the outstanding data buffer
      pdip_outstanding_buf_sz = pdip_bufsz * 2;
      pdip_outstanding_buf = (char *)malloc(pdip_outstanding_buf_sz);
      assert(pdip_outstanding_buf);

      // Open the input
      if (pScript)
      {
        pdip_in = open(pScript, O_RDONLY);
        if (pdip_in < 0)
        {
          err_sav = errno;
          PDIP_ERR("open('%s'): '%m' (%d)\n", pScript, errno);
          errno = err_sav;
          return 1;
        }
      }
      else
      {
        pdip_in = 0;
      }

      // Interact with the program
      if (options & 0x20)
      {
        rc = pdip_terminal();
      }
      else
      {
        rc = pdip_interact();
      }

      /*
        When the master device is closed, the process on the slave side gets
        the errno ENXIO when attempting a write() system call on the slave
        device but it will be able to read any data remaining on the slave
        stream. Finally, when all the data have been read, the read() system
        call will return 0 (zero) indicating that the slave can no longer be
        used.
      */
      if (options & 0x40)
      {
        pdip_dump_outstanding_data();
      }
      close(pdip_pty);
      pdip_pty = -1;
      free(pdip_buf);

      // Reset the signal SIGCHLD otherwise we may receive the end of child
      // with the following waitpid() and then we will receive the SIGCHLD
      // which would trigger the signal handler in which waitpid() would
      // return in error
      //
      // RECTIFICATION: We don't unset the SIGCHLD signal otherwise all
      //                subsequent calls to waitpid() fail with ECHILD and
      //                the sub-process is detached to belong to init and
      //                will live sometimes before the cyclic cleanup of init
      //
      //pdip_reset_sigchld();

      // If the child is still running
      if (!pdip_dead_prog)
      {
        PDIP_DBG(4, "Wait for end of program at most 3 seconds\n");

        // Install a timeout
        signal(SIGALRM, pdip_sig_alarm);
        alarm(3);

        // Get the status of the child if not dead yet
        (void)waitpid(-1, &status, 0);
      } // End if program is running

      // Free the parameters
      pdip_free_argv();

      // If timeout or syntax error or any other error
      if (rc != 0)
      {
        return 1;
      }

      // If sub-process terminated in error, propagate its exit code if
      // requested
      if (options & 0x80)
      {
        return pdip_exit_prog;
      }
    }
    break;
  } // End switch

  return 0;
} // main

