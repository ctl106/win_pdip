// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : pdip_lib.c
// Description : API for Programmed Dialogue with Interactive Programs
// License     :
//
//  Copyright (C) 2007-2018 Rachid Koucha <rachid dot koucha at gmail dot com>
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
//     06-Nov-2017 R. Koucha    - Creation
//     15-Jan-2018 R. Koucha    - Added pdip_fd()
//     22-Jan-2018 R. Koucha    - Added pdip_cpu_zero(), pdip_cpu_all()
//                              - Bug fix in pdip_cpu_alloc()
//     25-Jan-2018 R. Koucha    - Added PDIP_FLAG_RECV_ON_THE_FLOW
//                              - Redesign and bug fixes in pdip_recv()
//     30-Mar-2018 R. Koucha    - Redesigned the process termination to make it
//                                more flexible for users (e.g. ISYS and RSYS
//                                libraries)
//                              - pdip_exec(): bug fix as pdip_nb_cpu was not
//                                saved in child process
//     29-May-2018 R. Koucha    - "regoff_t" for result.rm_eo is either an
//                                "int" or a "long int" depending on the host
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#define _XOPEN_SOURCE
#define _GNU_SOURCE
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <string.h>
#include <regex.h>
#include <signal.h>
#include <sys/wait.h>
#include <ctype.h>
#include <stdarg.h>
#include <sched.h>

#include "pdip.h"
#include "pdip_p.h"
#include "pdip_util.h"

#include "plat_types.h"



/*

Notes:

  . The global mutex pdip_mtx is used to manage the list of objects
    as it is accessed by the user (pdip_new, pdip_delete) and
    the signal handler to look for the object concerned by the SIGCHLD

     So, to access the list of objects :

       PDIP_MASK_SIG(); // Disable signals first to avoid deadlock
                        // as signal handler may request the lock
       PDIP_LOCK();     // Mutual exclusion if multiple threads
                        // create/destroy objects concurrently

       ... Access to the list ...

       PDIP_UNLOCK();
       PDIP_UNMASK_SIG(); // Reactivate signal handler

  . Many threads may access the same object to check the state, status or
    get/send data from/to the controlled process. Meanwhile, the signal
    handler may update some fields in the object (state, status)

    So, to access those fields :

       PDIP_MASK_SIG(); // Disable signals first to avoid deadlock
                        // as signal handler may request the lock

       ... Access to the list ...

       PDIP_UNMASK_SIG(); // Reactivate signal handler


  . Concerning, the read/send data fields, if multiple threads access the
    same object, the mutual exclusion must be managed by the application
    as this seems to be a scarce not to say a weird behaviour.
    ==> This is a little like the access to a file. If multiple threads
        access the same file, they must manage the synchronization on
        their side

TODO:

  . Rewrite PDIP tool to make it use this PDIP API

*/


// ----------------------------------------------------------------------------
// Name   : pdip_nb_cpu
// Usage  : Number of CPUs
// ----------------------------------------------------------------------------
static unsigned int pdip_nb_cpu;


// ----------------------------------------------------------------------------
// Name   : pdip_debug_level
// Usage  : Global debug level
// ----------------------------------------------------------------------------
int pdip_debug_level;


// ----------------------------------------------------------------------------
// Name   : pdip_mtx
// Usage  : Mutual exclusion to access the list of objects
// ----------------------------------------------------------------------------
static pthread_mutex_t pdip_mtx;

#define PDIP_LOCK()   pthread_mutex_lock(&pdip_mtx)
#define PDIP_UNLOCK() pthread_mutex_unlock(&pdip_mtx)

// ----------------------------------------------------------------------------
// Name   : pdip_sigset
// Usage  : Set of signal to be masked/unmasked
// Note   : Signals must be masked before locking the mutex on the objects
//          and list otherwise we may trigger a deadlock with the signal
//          handler trying to get the lock while the application is locking
//          the list or an object
// ----------------------------------------------------------------------------
static sigset_t pdip_sigset;

#define PDIP_MASK_SIG()  pthread_sigmask(SIG_BLOCK, &pdip_sigset, 0)
#define PDIP_UNMASK_SIG()  pthread_sigmask(SIG_UNBLOCK, &pdip_sigset, 0)



// ----------------------------------------------------------------------------
// Name   : ctx_list
// Usage  : List of PDIP objects
// ----------------------------------------------------------------------------
static pdip_ctx_t *pdip_ctx_list;



// ----------------------------------------------------------------------------
// Name   : pdip_saved_action
// Usage  : Original signal disposition
// ----------------------------------------------------------------------------
struct sigaction pdip_saved_action;



//----------------------------------------------------------------------------
// Name        : pdip_write
// Description : Write out data
// Return      : len, if OK
//               -1, if error (errno is set)
//----------------------------------------------------------------------------
static int pdip_write(
                      pdip_ctx_t *ctxp,
                      const char *buf,
                      size_t      len
                     )
{
int          rc;
unsigned int l;
int          state;
int          err_sav;

  l = 0;
  do
  {
one_more_time:

    rc = write(ctxp->pty_master, buf + l, len - l);

    if (rc < 0)
    {
      if (EINTR == errno)
      {
        goto one_more_time;
      }

      err_sav = errno;

      PDIP_MASK_SIG();
      state = ctxp->state;
      PDIP_UNMASK_SIG();

      // The controlled program may be dead
      if (state != PDIP_STATE_ALIVE)
      {
        PDIP_ERR(ctxp, "Controlled program '%s' (%"PRIPID") is dead\n", ctxp->av[0], ctxp->pid);

        errno = err_sav;

        // Report an error or the amount of data written until now ?
        if (l)
	{
          return (int)l;
        }
        else
	{
          return -1;
	}
      }

      PDIP_ERR(ctxp, "write(%"PRISIZE"): '%m' (%d)\n", len - 1, errno);
      errno = err_sav;
      return -1;
    }

    assert((l + (size_t)rc) <= len);
    l += (size_t)rc;
  } while (l != len);

  return (int)len;

} // pdip_write


//----------------------------------------------------------------------------
// Name        : pdip_read
// Description : Read input data
// Return      : Number of read bytes if OK
//               -1, if error
//----------------------------------------------------------------------------
static int pdip_read(
                      pdip_ctx_t   *ctxp,
                      char         *buf,
                      size_t        l
                    )
{
int rc;
int err_sav;
int state;

  do
  {
    rc = read(ctxp->pty_master, buf, l);
    if (-1 == rc)
    {
      if (EINTR == errno)
      {
        continue;
      }

      err_sav = errno;

      PDIP_MASK_SIG();
      state = ctxp->state;
      PDIP_UNMASK_SIG();

      // The controlled program may be dead (but experimentations show that we may get the error
      // here before the signal handler updates the state of the object!)
      if (state != PDIP_STATE_ALIVE)
      {
        PDIP_DBG(ctxp, 1, "Controlled program '%s' (%"PRIPID") is dead\n", ctxp->av[0], ctxp->pid);
      }

      PDIP_DBG(ctxp, 1, "read(fd:%d, l:%"PRISIZE"): '%m' (%d), state=%d\n", ctxp->pty_master, l, errno, state);
      errno = err_sav;
      return -1;
    } // End if read error
  } while (rc < 0);

  return rc;
} // pdip_read



// ----------------------------------------------------------------------------
// Name   : PDIP_RESIZE_INCREMENT
// Usage  : Allocation increment for the receiving buffers
//          (must be bigger than 1 !)
// ----------------------------------------------------------------------------
#define PDIP_RESIZE_INCREMENT  1024



//----------------------------------------------------------------------------
// Name        : pdip_read_until_timeout
// Description : Read input data until a timeout occurs (if requested
//               by the user)
// Return      : 0, if OK
//               -1, if error (data may have been read !)
//----------------------------------------------------------------------------
static int pdip_read_until_timeout(
                                   pdip_ctx_t      *ctxp,
                                   char           **display,
                                   size_t          *display_sz,
                                   size_t          *data_sz,
                                   struct timeval  *to
                                  )
{
int             rc;
fd_set          fdset;
int             err_sav = 0;
size_t          len;

 PDIP_DBG(ctxp, 1, "Reading data from %"PRIPID", timeout %p (%lu s, %lu us)\n", ctxp->pid, to, (to ? to->tv_sec : 0), (to ? to->tv_usec : 0));

do_it_again:

  FD_ZERO(&fdset);
  FD_SET(ctxp->pty_master, &fdset);
  rc = select(ctxp->pty_master + 1, &fdset, 0, 0, to);
  switch(rc)
  {
    case -1:
    {
      err_sav = errno;
      if (EINTR == errno)
      {
        goto do_it_again;
      }

      PDIP_ERR(ctxp, "select(): '%m' (%d), data_sz=%"PRISIZE"\n", errno, *data_sz);
      rc = -1;
      goto end;
    }
    break;

    case 0: // Timeout
    {
      PDIP_DBG(ctxp, 5, "Timeout (data_sz=%"PRISIZE")\n", *data_sz);

      rc = 0;
      goto end;
    }
    break;

    default: // There are data
    {
      assert(FD_ISSET(ctxp->pty_master, &fdset));

      // If there is not enough space in the buffer, enlarge it
      if ((*display_sz - *data_sz) < ctxp->buf_resize_increment)
      {
        *display_sz += ctxp->buf_resize_increment;
        *display = (char *)realloc(*display, *display_sz);
        if (!(*display))
        {
          // Errno is set
          err_sav = errno;
          *display_sz = *data_sz = 0;
          return -1;
        }
      } // End if not enough space in the buffer

      // Read the data, keep one slot for terminating NUL
      len = (*display_sz) - (*data_sz) - 1;
      rc = pdip_read(ctxp, *display + *data_sz, len);
      err_sav = errno;
      PDIP_DBG(ctxp, 3, "Read %d bytes from process %"PRIPID" in buffer %p at offset %"PRISIZE"\n", rc, ctxp->pid, *display, *data_sz);
      if (rc >= 0)
      {
        // If rc == 0 : No more data to read (i.e. An error occured while reading
        // or the controlled process is dead)

        (*data_sz) += rc;

        // NUL terminate the string
        (*display)[*data_sz] = '\0';

        rc = 0;
      }
      else // Error
      {
        // NUL terminate the string (data read until now)
        (*display)[*data_sz] = '\0';

        PDIP_DBG(ctxp, 1, "Error '%m' (%d), data_sz=%"PRISIZE"\n", errno, *data_sz);

        // errno is set
        rc = -1;
      }
    } // break;
  } // End switch

end:

  errno = err_sav;

  return rc;

} // pdip_read_until_timeout


// ----------------------------------------------------------------------------
// Name   : pdip_flush_internal
// Usage  : Flush the outstanding data
// Return : 0, if OK
//          -1, if error
// ----------------------------------------------------------------------------
static int pdip_flush_internal(
 	                       pdip_ctx_t  *ctxp,
                               char       **display,
                               size_t      *display_sz,
                               size_t      *data_sz
                              )
{
  if (ctxp->outstanding_data)
  {
    if (*display_sz)
    {
      assert(display && *display);
      (void)free(*display);
    }

    // The outstanding data is NUL terminated
    assert('\0' == *(ctxp->outstanding_data + ctxp->outstanding_data_offset));
    *display = ctxp->outstanding_data;
    *data_sz = ctxp->outstanding_data_offset;
    *display_sz = ctxp->outstanding_data_sz;

    ctxp->outstanding_data        = (char *)0;
    ctxp->outstanding_data_offset = 0;
    ctxp->outstanding_data_sz     = 0;
  }
  else // No outstanding data
  {
    assert(0 == ctxp->outstanding_data_offset);
    assert(0 == ctxp->outstanding_data_sz);
    *data_sz = 0;
  } // End if outstanding data

  return 0;
} // pdip_flush_internal




// ----------------------------------------------------------------------------
// Name   : pdip_flush_outstanding
// Usage  : Flush the outstanding data except the last line if it is not
//          complete as the following unread yet data may complete the line and
//          make the regex match
// Return : 0, if at least one line flushed
//          1, no line to flush
//          -1, if error
// ----------------------------------------------------------------------------
static int pdip_flush_outstanding(
 	                          pdip_ctx_t  *ctxp,
                                  char       **display,
                                  size_t      *display_sz,
                                  size_t      *data_sz
                                 )
{
size_t  len;
 char   *p, *p1, *pEnd;

  assert(ctxp->outstanding_data);

  // If there are no outstanding data
  if (!(ctxp->outstanding_data_offset))
  {
    return 1;
  }

  pEnd = ctxp->outstanding_data + ctxp->outstanding_data_offset;
  assert('\0' == *pEnd);

  // Look for the end of the last complete line
  p = pEnd - 1;
  while ((p != ctxp->outstanding_data) && (*p != '\n'))
  {
    p --;
  } // End while

  // If there is not a complete line
  if ('\n' != *p)
  {
    return 1;
  }

  // Go after the '\n'
  p ++;

  // Amount of data to return (including the terminating NUL)
  len = (p - ctxp->outstanding_data) + 1;

  // If at least one byte (not including terminating NUL) can be returned
  PDIP_DBG(ctxp, 5, "len=%"PRISIZE" + 1\n", len - 1);

  // If there are remaining data behind the last line
  if (p != pEnd)
  {
    p1 = strdup(p);
    if (!p1)
    {
      // errno is set
      return -1;
    }
  }
  else
  {
    // To avoid compiler complaints
    p1 = (char *)0;
  }

  // Return the beginning of the outstanding data
  *display = ctxp->outstanding_data;
  *display_sz = ctxp->outstanding_data_sz; // The physical size of the buffer
  *data_sz = p - ctxp->outstanding_data; // The size of the complete lines

  // NUL terminate the data
  *p = '\0';

  if (p != pEnd)
  {
    // Make the outstanding data refer to the duplicated end of original buffer
    // (i.e. the remaining data behind the last complete line)
    ctxp->outstanding_data_sz = (pEnd - p) + 1;
    ctxp->outstanding_data = p1;
    ctxp->outstanding_data_offset = ctxp->outstanding_data_sz - 1;
    assert(strlen(p1) == ctxp->outstanding_data_offset);
  }
  else
  {
    ctxp->outstanding_data_sz = ctxp->outstanding_data_offset = 0;
    ctxp->outstanding_data = (char *)0;
  }

  return 0;
} // pdip_flush_outstanding



//----------------------------------------------------------------------------
// Name        : pdip_append_to_outstanding
// Description : Put data in outstanding space
// Return      : 0, if OK
//              -1, if error
//----------------------------------------------------------------------------
static int pdip_append_to_outstanding(
                                      pdip_ctx_t  *ctxp,
                                      char       **display,
                                      size_t      *display_sz,
                                      size_t      *data_sz
		   		     )
{
int err_sav;

  PDIP_DBG(ctxp, 5, "Appending %"PRISIZE" bytes to outstanding data (%p) at offset %"PRISIZE"\n", *data_sz, ctxp->outstanding_data, ctxp->outstanding_data_offset);

  if (!(*data_sz))
  {
    return 0;
  }

  // There is always a terminating NUL not counted by data_sz
  assert(*display_sz > *data_sz);
  pdip_assert('\0' == (*display)[*data_sz], "*display_sz=%"PRISIZE", *data_sz=%"PRISIZE"\n", *display_sz, *data_sz);

  //printf("outstanding_data_sz=%"PRISIZE", outstanding_data_offset=%"PRISIZE", *display_sz=%"PRISIZE", *data_sz=%"PRISIZE"\n", ctxp->outstanding_data_sz, ctxp->outstanding_data_offset, *display_sz, *data_sz);

  // If there are already outstanding data
  if (ctxp->outstanding_data)
  {
    assert(ctxp->outstanding_data_sz > 0);
    pdip_assert('\0' == ctxp->outstanding_data[ctxp->outstanding_data_offset], "outstanding_data_sz=%"PRISIZE", outstanding_data_offset=%"PRISIZE", *display_sz=%"PRISIZE", *data_sz=%"PRISIZE"\n", ctxp->outstanding_data_sz, ctxp->outstanding_data_offset, *display_sz, *data_sz);
    assert(ctxp->outstanding_data_offset <= ctxp->outstanding_data_sz);

    if (*data_sz > (ctxp->outstanding_data_sz - (ctxp->outstanding_data_offset + 1)))
    {
      // Terminating NUL is already in ctxp->outstanding_data_sz
      ctxp->outstanding_data = (char *)realloc(ctxp->outstanding_data, ctxp->outstanding_data_sz + *data_sz);
      if (!(ctxp->outstanding_data))
      {
        // Errno is set
        err_sav = errno;
        PDIP_ERR(ctxp, "realloc(%"PRISIZE"): '%m' (%d)\n", ctxp->outstanding_data_sz + *data_sz, errno);
        ctxp->outstanding_data_sz = 0;
        ctxp->outstanding_data_offset = 0;
        errno = err_sav;
        return -1;
      }

      ctxp->outstanding_data_sz += *data_sz;
    } // End if enough space

    // Include terminating NUL in the copy
    memcpy(ctxp->outstanding_data + ctxp->outstanding_data_offset, *display, *data_sz + 1);
    ctxp->outstanding_data_offset += *data_sz;
  }
  else
  {
    assert(0 == ctxp->outstanding_data_sz);
    assert(0 == ctxp->outstanding_data_offset);

    // Add terminating NUL
    ctxp->outstanding_data = *display;
    ctxp->outstanding_data_sz = *display_sz;
    ctxp->outstanding_data_offset = *data_sz;
    assert('\0' == ctxp->outstanding_data[ctxp->outstanding_data_offset]);
  }

  // Reset the user data
  *display = (char *)0;
  *display_sz = 0;
  *data_sz = 0;

  return 0;
} // pdip_append_to_outstanding


//----------------------------------------------------------------------------
// Name        : pdip_look_for_regex
// Description : Handle the synchronization string
// Note        : NUL chars are removed
//
// Return      : 0, if synchro string found
//               1, if synchro string not found
//              -1, if error
//----------------------------------------------------------------------------
static int pdip_look_for_regex(
                               pdip_ctx_t    *ctxp,
                               const char    *regular_expr,
                               regex_t       *regex,
                               char         **display,
                               size_t        *display_sz,
                               size_t        *data_sz
                              )
{
int        rc;
regmatch_t result;

  if (*data_sz)
  {
    // Append the incoming data to the outstanding one
    // If the last line in the outstanding was incomplete, the incoming data may
    // complete it
    rc = pdip_append_to_outstanding(ctxp, display, display_sz, data_sz);
    if (rc != 0)
    {
      // Errno is set
      return -1;
    }
  } // End if incoming data to add to the outstanding ones

  // If there are outstanding data
  if (ctxp->outstanding_data_offset > 0)
  {
    PDIP_DUMP(ctxp, 2, "Looking for a match of <%s> in (%p):\n", ctxp->outstanding_data, ctxp->outstanding_data_offset, regular_expr, ctxp->outstanding_data);

    // Look for the regular expression in the outstanding data
    rc = regexec(regex, ctxp->outstanding_data, 1, &result, 0);
    if (0 == rc)
    {
    char   *p;
    size_t  l;

      // The relative 'rm_eo' field indicates the end offset of the match
      // If 'rm_eo' == 0, this means that we are matching a beginning or end
      // of line
      // regoff_t type of result.rm_eo is sometimes "int" and sometimes
      // "long int". Hence, the cast to "long int"
      PDIP_DBG(ctxp, 2, "Pattern matching SUCCEEDED at offset %ld in outstanding data !\n", (long int)(result.rm_eo));

      if (0 == result.rm_eo)
      {
	// Here, we are sure that there is at least one
        // byte (ctxp->outstanding_data_offset > 0)
        if ('\n' == ctxp->outstanding_data[0])
	{
          // Skip the new line
          PDIP_DBG(ctxp, 2, "Match at offset 0 ==> Skipping end of line !\n");
          result.rm_eo = 1;
	}
      } // End if match beginning/end of line

      // The data up to the end offset of the matching pattern must be
      // returned to the user
      // The remainning data stays in the outstanding
      *display = ctxp->outstanding_data;
      *display_sz = ctxp->outstanding_data_sz;
      // The outstanding data is NUL terminated
      assert('\0' == ctxp->outstanding_data[ctxp->outstanding_data_offset]);
      *data_sz = result.rm_eo;

      p = ctxp->outstanding_data + *data_sz;

      // Size of the remaining data
      l = ctxp->outstanding_data_offset - result.rm_eo;
      pdip_assert(strlen(p) == l, "l=%"PRISIZE", strlen(p)=%"PRISIZE"\n", l, strlen(p));

      // Reset the outstanding data space
      ctxp->outstanding_data_offset = 0;
      ctxp->outstanding_data = (char *)0;
      ctxp->outstanding_data_sz = 0;

      // Append the remaining data in the outstanding space
      // When the outstanding data is empty, the following function merely makes
      // the user data becoming the outstanding data without copy. So, we need
      // to duplicate this space here as we need the beginning of a dynamic memory buffer
      // (otherwise the call to "free()" to delete the content of the outstanding data
      // would crash
      if (l)
      {
      size_t  l1;

        p = strdup(p);
        if (rc < 0)
        {
          // Errno is set
          *display = (char *)0;
          *data_sz = 0;
          return -1;
        }
        l1 = l + 1;
        rc = pdip_append_to_outstanding(ctxp, &p, &l1, &l);
        if (rc < 0)
        {
          // Errno is set
          *display = (char *)0;
          *data_sz = 0;
          return -1;
        }

        assert(0 == l);
        assert(0 == l1);
      } // End if remaining outstanding data

      // There is at least one slot for the terminating NUL in the outstanding space;
      (*display)[result.rm_eo] = '\0';

      PDIP_DUMP(ctxp, 2, "Remaining outstanding data (%"PRISIZE" bytes):\n", ctxp->outstanding_data, ctxp->outstanding_data_offset, ctxp->outstanding_data_offset);

      return 0;
    }
    else // Regex not found
    {
      PDIP_DBG(ctxp, 2, "Pattern matching FAILED!\n");

      // The data is kept in the outstanding space

    } // End if regex found
  }
  else
  {
    PDIP_DBG(ctxp, 2, "No outstanding data to look for a match of <%s>\n", regular_expr);
  } // End if outstanding data

  return 1;
} // pdip_look_for_regex





// ----------------------------------------------------------------------------
// Name   : pdip_recv
// Usage  : Receive data from the controlled process
//          If the timeout is NULL and the regular expression is not found,
//          the function blocks indefinitely
// Return : PDIP_RECV_FOUND
//          PDIP_RECV_TIMEOUT
//          PDIP_RECV_DATA
//          PDIP_RECV_ERROR (errno is set)
// ----------------------------------------------------------------------------
int pdip_recv(
              pdip_t          *ctx,
	      const char      *regular_expr,
              char           **display,
              size_t          *display_sz,
              size_t          *data_sz,      // OUT: strlen() of the received data (i.e. Terminating NUL not counted)
              struct timeval  *timeout
             )
{
int            rc;
pdip_ctx_t    *ctxp;
int            err_sav = 0;

  if (!data_sz)
  {
    errno = EINVAL;
    return PDIP_RECV_ERROR;
  }

  // No data for the moment
  *data_sz = 0;

  if (!display || !display_sz || !ctx)
  {
    errno = EINVAL;
    return PDIP_RECV_ERROR;
  }

  // Some coherency checks
  if ((*display_sz && !(*display)) ||
      (!(*display_sz) && *display))
  {
    errno = EINVAL;
    return PDIP_RECV_ERROR;
  }

  ctxp = (pdip_ctx_t *)ctx;

  // The process may be dead but there may be outstanding data
  // in internal buffers or PTY. Hence we don't check ALIVE state
  // but the validity of the file descriptor on the master side
  // The file descriptor on the master side is equal to -1
  // only if the object is not linked to any process (i.e. pdip_exec()
  // not called or process is dead)
  // No need for mutual exclusion with the signal handler as the
  // user is supposed to make one call at a time on a given object
  // (eventually making mutual exclusion on its side if multiple threads
  // access the same object)
  if (ctxp->pty_master < 0)
  {
    errno = EPERM;
    return PDIP_RECV_ERROR;
  }

  rc = PDIP_RECV_ERROR;

  // If a regular expression is not passed for synchronization
  if (!regular_expr)
  {
    // Flush the outstanding data
    rc = pdip_flush_internal(ctxp, display, display_sz, data_sz);
    if (rc < 0)
    {
      // Errno is set
      err_sav = errno;
      rc = PDIP_RECV_ERROR;
      goto end_no_regex;
    }

    // If outstanding data have been flushed ==> Return them to the user
    if (*data_sz)
    {
      PDIP_DBG(ctxp, 4, "Returning %"PRISIZE" outstanding data bytes\n", *data_sz);
      rc = PDIP_RECV_DATA;
      goto end_no_regex;
    }

    // If a timeout is passed
    if (timeout)
    {
      // Get new data if any
      rc = pdip_read_until_timeout(ctxp, display, display_sz, data_sz, timeout);
      err_sav = errno;

      switch(rc)
      {
        case 0: // No error
        {
          // If data have been read
          if (*data_sz > 0)
	  {
            rc = PDIP_RECV_DATA;
	  }
          else // No data
	  {
            rc = PDIP_RECV_TIMEOUT;
	  }
        }
        break;

        case -1: // Error
        {
          // Data may have been read (data_sz >= 0)
          // If we received data, return OK!
          if (*data_sz)
	  {
            rc = PDIP_RECV_DATA;
	  }
          else
	  {
            err_sav = errno;
            rc = PDIP_RECV_ERROR;
	  }
        }
        break;

        default: // Bug !?!
        {
          assert(0);
          err_sav = errno;
          rc = PDIP_RECV_ERROR;
        }
      } // End switch
    }
    else // No timeout
    {
      // If there is not enough space in the buffer, enlarge it
      if (*display_sz < ctxp->buf_resize_increment)
      {
        *display_sz += ctxp->buf_resize_increment;
        *display = (char *)realloc(*display, *display_sz);
        if (!(*display))
        {
          *display_sz = *data_sz = 0;
          // Errno is set
          err_sav = errno;
          rc = PDIP_RECV_ERROR;
          goto end_no_regex;
        }
      } // End if not enough space in the buffer

      // Blocking data reception (let 1 slot for terminating NUL)
      rc = pdip_read(ctxp, *display, *display_sz - 1);
      if (rc > 0)
      {
        (*display)[rc] = '\0';
        *data_sz = rc;
        rc = PDIP_RECV_DATA;
      }
      else // Error
      {
        assert(0 != rc);

        err_sav = errno;
        rc = PDIP_RECV_ERROR;
      }
    } // End if timeout

end_no_regex:

    PDIP_DBG(ctxp, 5, "Return code: %d\n", rc);

    errno = err_sav;

    return rc;
  }
  else // A regular expression has been passed
  {
  regex_t regex;
  char    regex_err[256];

    memset(&regex, 0, sizeof(regex));

    // Compile the regular expression
    //
    // . After compilation, the compiler returns the number of parenthesized
    //   subexpressions in regex.re_nsub
    //
    PDIP_DBG(ctxp, 3, "Compiling <%s>\n", regular_expr);
    rc = regcomp(&regex, regular_expr, REG_EXTENDED|REG_NEWLINE);
    //rc = regcomp(&regex, regular_expr, REG_EXTENDED);
    if (0 != rc)
    {
      (void)regerror(rc, &regex, regex_err, sizeof(regex_err));
      PDIP_ERR(ctxp, "Bad regular expression <%s>: %s\n", regular_expr, regex_err);
      err_sav = EINVAL;
      rc = PDIP_RECV_ERROR;
      goto end_regex;
    }

    PDIP_DBG(ctxp, 5, "Number of sub expressions in regex (%s): %"PRISIZE"\n", regular_expr, regex.re_nsub);

    // First of all, look for a match in the outstanding data if any
    rc = pdip_look_for_regex(ctxp, regular_expr, &regex, display, display_sz, data_sz);

    // If pattern matching succeeded
    if (0 == rc)
    {
      rc = PDIP_RECV_FOUND;
      goto end_regex;
    } // End if pattern matching succeeded

    if (timeout)
    {

read_again:

      rc = pdip_read_until_timeout(ctxp, display, display_sz, data_sz, timeout);
      err_sav = errno;

      switch(rc)
      {
        case 0: // No error
        {
          // If data have been read
          if (*data_sz > 0)
	  {
            // Look for the regular expression
            rc = pdip_look_for_regex(ctxp, regular_expr, &regex, display, display_sz, data_sz);
            switch(rc)
            {
              case 0 : // Regular expression found
              {
                // The read data located after the pattern matching is stored in the
                // outstanding data of the context.
                rc = PDIP_RECV_FOUND;
                goto end_regex;
              }
              break;
              case 1 : // Regular expression not found
              {
                // If timeout not elapsed
                // Linux select() updates the timeout parameter with the remaining time
                if (timeout->tv_sec || timeout->tv_usec)
	        {
                  // Reiterate the read with the remaining timeout only if flag
                  // RECV_ON_THE_FLOW is not set
                  if (ctxp->flags & PDIP_FLAG_RECV_ON_THE_FLOW)
		  {
                    PDIP_DBG(ctxp, 7, "Flag RECV_ON_THE_FLOW is set ==> Trying an intermediate return of outstanding data\n");

                    // The user wants to receive intermediate data
                    // ==> Return the data read until now except the last line if it is not complete
                    //     as the following data may complete the line and make the regex match
                    rc = pdip_flush_outstanding(ctxp, display, display_sz, data_sz);
                    switch(rc)
		    {
   		      case 0: // Outstanding data to return
		      {
                        rc = PDIP_RECV_DATA;
                        goto end_regex;
		      }
                      break;

		      case 1: // Not enough outstanding data (at least not a complete line to return)
		      {
                        // Loop again to read more data as the timeout is not elapsed
                        goto read_again;
		      }
                      break;

	  	      default : // Error
		      {
                        err_sav = errno;
                        rc = PDIP_RECV_ERROR;
                        goto end_regex;
		      }
                      break;
		    } // End switch

		  }
                  else // No reception on the flow
		  {
                    // Reiterate the read
                    goto read_again;
		  }
	        }
                else // Timeout elapsed
	        {
                  // This is a scarce (impossible) case where we received data
                  // and the timeout dropped to 0. I did not succeed to reproduce
                  // this in unitary tests

                  // Return outstanding data only if RECV_ON_THE_FLOW is set
                  if ((ctxp->flags & PDIP_FLAG_RECV_ON_THE_FLOW) && (ctxp->outstanding_data_offset))
		  {
                    // The timeout expired, return the data read until now
                    // ==> Transfer the outstanding data into the user buffer
                    rc = pdip_flush_internal(ctxp, display, display_sz, data_sz);
                    if (rc != 0)
	            {
                      // errno is set
                      err_sav = errno;
                      rc = PDIP_RECV_ERROR;
	            }
                    else
		    {
                      PDIP_DBG(ctxp, 4, "Returning %"PRISIZE" outstanding data bytes\n", *data_sz);
                      rc = PDIP_RECV_DATA;
		    }
	          }
                  else // No data and/or !RECV_ON_THE_FLOW
	          {
                    rc = PDIP_RECV_TIMEOUT;
	          }
		} // End if not timeout
              }
              break;
              default : // Error
              {
                // Data may have been read (data_sz >= 0)
                err_sav = errno;
                rc = PDIP_RECV_ERROR;
                goto end_regex;
              }
              break;
            } // End switch
	  }
          else // No data
	  {

            // The timeout elapsed
            pdip_assert(0 == timeout->tv_sec && 0 == timeout->tv_usec, "sec=%lu, usec=%lu\n", timeout->tv_sec, timeout->tv_usec);
            rc = PDIP_RECV_TIMEOUT;
	  }
        }
        break;

        case -1: // Error
        {
          // Data may have been read (data_sz >= 0)
          // If we received data, return OK!
          if (*data_sz)
	  {
            rc = PDIP_RECV_DATA;
	  }
          else
	  {
            err_sav = errno;
            rc = PDIP_RECV_ERROR;
	  }
        }
        break;

        default: // Bug !?!
        {
          assert(0);
          err_sav = errno;
          rc = PDIP_RECV_ERROR;
        }
      } // End switch
    }
    else // No timeout
    {
      while(1)
      {
        // If there is not enough space in the buffer, enlarge it
        if ((*display_sz - *data_sz) < ctxp->buf_resize_increment)
        {
          *display_sz += ctxp->buf_resize_increment;
          *display = (char *)realloc(*display, *display_sz);
          if (!(*display))
          {
            *display_sz = *data_sz = 0;
            // Errno is set
            err_sav = errno;
            rc = PDIP_RECV_ERROR;
            goto end_regex;
          }

          PDIP_DBG(ctxp, 8, "Realloced reception buffer %p with %"PRISIZE" bytes (data_sz=%"PRISIZE")\n", *display, *display_sz, *data_sz);
        } // End if not enough space in the buffer

        // Blocking data reception (let one slot for terminating NUL)
        rc = pdip_read(ctxp, *display, *display_sz - 1);
        PDIP_DBG(ctxp, 8, "pdip_read() = %d\n", rc);
        if (rc > 0)
        {
          *data_sz = rc;

          // NUL terminate the string
          (*display)[*data_sz] = '\0';

          // Append data to the outstanding space and look for the regular expression
          rc = pdip_look_for_regex(ctxp, regular_expr, &regex, display, display_sz, data_sz);
          switch(rc)
	  {
  	    case 0: // Regex found
	    {
              // The read data located after the pattern matching is stored in the
              // outstanding data of the context.
              rc = PDIP_RECV_FOUND;
              goto end_regex;
	    }
            break;

	    case 1 : // Regex not found
	    {
              // Data read until now does not contain the regular expression
              if (ctxp->flags & PDIP_FLAG_RECV_ON_THE_FLOW)
	      {
                // The user wants to receive intermediate data
                // ==> Return the data read until now except the last line if it is not complete
                //     as the following data may complete the line and make the regex match
                rc = pdip_flush_outstanding(ctxp, display, display_sz, data_sz);
                PDIP_DBG(ctxp, 10, "pdip_flush_outstanding()=%d\n", rc);
                switch(rc)
		{
   		  case 0: // Outstanding data to return
		  {
                    rc = PDIP_RECV_DATA;
                    goto end_regex;
		  }
                  break;

		  case 1: // Not enough outstanding data (at least not a complete line to return)
		  {
                    // Loop again to read more data as the timeout is infinite
		  }
                  break;

		  default : // Error
		  {
                    err_sav = errno;
                    rc = PDIP_RECV_ERROR;
                    goto end_regex;
		  }
                  break;
		} // End switch
	      }
              else // ! RECV_ON_THE_FLOW
	      {
                // Loop again to read more data as the timeout is infinite

	      } // RECV_ON_THE_FLOW
	    }
            break;

	    case -1 : // Error
	    {
              err_sav = errno;
              rc = PDIP_RECV_ERROR;
              goto end_regex;
	    }
            break;

	    default: // Bug
	    {
              assert(0);
	    }
            break;
	  } // End switch
        }
        else // Error
        {
          assert(0 != rc);

          err_sav = errno;
          rc = PDIP_RECV_ERROR;
          goto end_regex;
        }
      } // End while

    } // End if timeout

end_regex:

    PDIP_DBG(ctxp, 5, "Return code: %d\n", rc);

    regfree(&regex);

    errno = err_sav;
    return rc;

  } // End if regular expression

} // pdip_recv



// ----------------------------------------------------------------------------
// Name   : pdip_send
// Usage  : Send a formated string to the controlled process
// Return : Amount of sent data, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int pdip_send(
              pdip_t      ctx,
              const char *format,
              ...
             )
{
pdip_ctx_t *ctxp;
int         rc;
char        str[4096];
va_list     ap;
int         state;

  if (!ctx || !format)
  {
    errno = EINVAL;
    return -1;
  }

  ctxp = (pdip_ctx_t *)ctx;

  // Mutual exclusion with the signal handler
  PDIP_MASK_SIG();
  state = ctxp->state;
  PDIP_UNMASK_SIG();

  if (PDIP_STATE_ALIVE != state)
  {
    PDIP_ERR(ctxp, "Controlled process is not alive\n");
    errno = EPERM;
    return -1;
  }

  va_start(ap, format);
  rc = vsnprintf(str, sizeof(str), format, ap);
  va_end(ap);

  if (rc < 0)
  {
    // errno is set
    return -1;
  }
  else if ((size_t)(rc + 1) > sizeof(str))
  {
    PDIP_ERR(ctxp, "format '%s' is too long (rc=%d) for internal buffer (%"PRISIZE" bytes max)\n", format, rc, sizeof(str));
    errno = ENOSPC;
    return -1;
  }

  PDIP_DBG(ctxp, 2, "Sending %d bytes: '%s'\n", rc, str);

  rc = pdip_write(ctxp, str, rc);

  // In case of error, errno is set

  return rc;
} // pdip_send




// ----------------------------------------------------------------------------
// Name   : pdip_display_term_settings
// Usage  : Display the terminal settings
// ----------------------------------------------------------------------------
static void pdip_display_term_settings(
                                       const char     *side,
                                       struct termios *term_settings
                                      )
{
  fprintf(stderr, "PTY settings on %s side:\n"
                  "\tInput modes: %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s\n"
                  "\tOutput modes: %s %s %s %s %s %s %s %s %s %s %s %s %s %s\n"
                  "\tLocal modes: %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s\n"
                ,
          side,
	  term_settings->c_iflag & IGNBRK ? "IGNBRK" : ".",
	  term_settings->c_iflag & BRKINT ? "BRKINT" : ".",
	  term_settings->c_iflag & IGNPAR ? "IGNPAR" : ".",
	  term_settings->c_iflag & PARMRK ? "PARMRK" : ".",
	  term_settings->c_iflag & INPCK ? "INPCK" : ".",
	  term_settings->c_iflag & ISTRIP ? "ISTRIP" : ".",
	  term_settings->c_iflag & INLCR ? "INLCR" : ".",
	  term_settings->c_iflag & IGNCR ? "IGNCR" : ".",
	  term_settings->c_iflag & ICRNL ? "ICRNL" : ".",
	  term_settings->c_iflag & IUCLC ? "IUCLC" : ".",
	  term_settings->c_iflag & IXON ? "IXON" : ".",
	  term_settings->c_iflag & IXANY ? "IXANY" : ".",
	  term_settings->c_iflag & IXOFF  ? "IXOFF" : ".",
	  term_settings->c_iflag & IMAXBEL ? "IMAXBEL" : ".",
	  term_settings->c_iflag & IUTF8 ? "IUTF8" : ".",
	  term_settings->c_oflag & OPOST  ? "OPOST" : ".",
	  term_settings->c_oflag & OLCUC ? "OLCUC" : ".",
	  term_settings->c_oflag & ONLCR ? "ONLCR" : ".",
	  term_settings->c_oflag & OCRNL ? "OCRNL" : ".",
	  term_settings->c_oflag & ONOCR ? "ONOCR" : ".",
	  term_settings->c_oflag & ONLRET ? "ONLRET" : ".",
	  term_settings->c_oflag & OFILL ? "OFILL" : ".",
	  term_settings->c_oflag & OFDEL ? "OFDEL" : ".",
	  term_settings->c_oflag & NLDLY ? "NLDLY" : ".",
	  term_settings->c_oflag & CRDLY ? "CRDLY" : ".",
	  term_settings->c_oflag & TABDLY ? "TABDLY" : ".",
	  term_settings->c_oflag & BSDLY ? "BSDLY" : ".",
	  term_settings->c_oflag & VTDLY ? "VTDLY" : ".",
	  term_settings->c_oflag & FFDLY ? "FFDLY" : ".",
	  term_settings->c_lflag & ISIG ? "ISIG" : ".",
	  term_settings->c_lflag & ICANON ? "ICANON" : ".",
	  term_settings->c_lflag & XCASE ? "XCASE" : ".",
	  term_settings->c_lflag & ECHO ? "ECHO" : ".",
	  term_settings->c_lflag & ECHOE ? "ECHOE" : ".",
	  term_settings->c_lflag & ECHOK ? "ECHOK" : ".",
	  term_settings->c_lflag & ECHONL ? "ECHONL" : ".",
	  term_settings->c_lflag & ECHOCTL ? "ECHOCTL" : ".",
	  term_settings->c_lflag & ECHOPRT ? "ECHOPRT" : ".",
	  term_settings->c_lflag & ECHOKE ? "ECHOKE" : ".",
	  term_settings->c_lflag & FLUSHO ? "FLUSHO" : ".",
	  term_settings->c_lflag & NOFLSH ? "NOFLSH" : ".",
	  term_settings->c_lflag & TOSTOP ? "TOSTOP" : ".",
	  term_settings->c_lflag & PENDIN ? "PENDIN" : ".",
	  term_settings->c_lflag & IEXTEN ? "IEXTEN" : "."
         );
} // pdip_display_term_settings



// ----------------------------------------------------------------------------
// Name   : pdip_init_ctx
// Usage  : Initialize the fields of an object
// Return : None
// ----------------------------------------------------------------------------
static void pdip_init_ctx(
                          pdip_ctx_t *ctxp
                         )
{
  ctxp->av                      = (char **)0;
  ctxp->ac                      = 0;
  ctxp->pty_master              = -1;
  ctxp->debug                   = 0;
  ctxp->pid                     = -1;
  ctxp->status                  = 0;
  ctxp->state                   = PDIP_STATE_INIT;
  ctxp->outstanding_data        = 0;
  ctxp->outstanding_data_sz     = 0;
  ctxp->outstanding_data_offset = 0;
  ctxp->dbg_output              = stderr;
  ctxp->err_output              = stderr;
  ctxp->flags                   = 0;
  ctxp->cpu                     = (unsigned char *)0;
  ctxp->buf_resize_increment    = PDIP_RESIZE_INCREMENT;

  // Don't touch prev & next pointers
} // pdip_init_ctx


// ----------------------------------------------------------------------------
// Name   : pdip_free_resources
// Usage  : Free the resources of a context
// Return : None
// ----------------------------------------------------------------------------
static void pdip_free_resources(pdip_ctx_t *ctxp)
{
  // For debug purposes, we reset the fields in
  // case the user would reuse them after deallocation

  if (ctxp->av)
  {
  unsigned int i;

    for (i = 0; i < (unsigned)(ctxp->ac); i++)
    {
      // In case the allocation failed
      if (ctxp->av[i])
      {
        free(ctxp->av[i]);
      }
    } // End for

    free(ctxp->av);
  } // End if

  if (ctxp->pty_master >= 0)
  {
    (void)close(ctxp->pty_master);
  }

  if (ctxp->outstanding_data)
  {
    free(ctxp->outstanding_data);
  }

  if (ctxp->cpu)
  {
    (void)pdip_cpu_free(ctxp->cpu);
  }

  pdip_init_ctx(ctxp);

  // If linked, the context is not unlinked
  // So, we don't touch ctxp->prev & ctxp->next
} // pdip_free_resources







#define PDIP_NB_BYTES_FOR_CPU() ((pdip_nb_cpu + 0x7) >> 3)

// ----------------------------------------------------------------------------
// Name   : pdip_cpu_nb
// Usage  : Return the number of CPUs
// Return : Number of CPUs
// ----------------------------------------------------------------------------
unsigned int pdip_cpu_nb(void)
{
  return pdip_nb_cpu;
} // pdip_cpu_nb


// ----------------------------------------------------------------------------
// Name   : pdip_cpu_alloc
// Usage  : Allocate a bitmap of CPUs
// Return : bitmap, if OK
//          0, if error (errno is set)
// ----------------------------------------------------------------------------
unsigned char *pdip_cpu_alloc(void)
{
unsigned char *cpu = (unsigned char *)malloc(PDIP_NB_BYTES_FOR_CPU() * sizeof(unsigned char *));

  if (cpu)
  {
    memset(cpu, 0, PDIP_NB_BYTES_FOR_CPU());

    // By default, we don't set any CPU in the bitmap: the system's defaults will
    // apply
  }

  return cpu;

} // pdip_cpu_alloc


// ----------------------------------------------------------------------------
// Name   : pdip_cpu_free
// Usage  : Free a bitmap of CPUs
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int pdip_cpu_free(unsigned char *cpu)
{
  if (cpu)
  {
    free(cpu);
    return 0;
  }

  errno = EINVAL;
  return -1;
} // pdip_cpu_free


// ----------------------------------------------------------------------------
// Name   : pdip_cpu_zero
// Usage  : Reset a bitmap of CPUs
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int pdip_cpu_zero(unsigned char *cpu)
{
  if (!cpu)
  {
    errno = EINVAL;
    return -1;
  }

  memset(cpu, 0, PDIP_NB_BYTES_FOR_CPU());

  return 0;

} // pdip_cpu_zero

// ----------------------------------------------------------------------------
// Name   : pdip_cpu_all
// Usage  : Set all the bits in a bitmap of CPUs
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int pdip_cpu_all(unsigned char *cpu)
{
  if (!cpu)
  {
    errno = EINVAL;
    return -1;
  }

  memset(cpu, 0xFF, PDIP_NB_BYTES_FOR_CPU());

  return 0;

} // pdip_cpu_all


// ----------------------------------------------------------------------------
// Name   : pdip_cpu_set
// Usage  : Set the CPU number n in the bitmap
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int pdip_cpu_set(
                  unsigned char *cpu,
                  unsigned int   n
                 )
{
  if (n >= pdip_nb_cpu)
  {
    errno = EINVAL;
    return -1;
  }

  cpu[n >> 3] |=  (1 << (n & 0x7));

  return 0;
} // pdip_cpu_set


// ----------------------------------------------------------------------------
// Name   : pdip_cpu_unset
// Usage  : Unset the CPU number n in the bitmap
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int pdip_cpu_unset(
                   unsigned char *cpu,
                   unsigned int   n
                  )
{
  if (n >= pdip_nb_cpu)
  {
    errno = EINVAL;
    return -1;
  }

  cpu[n >> 3] &=  ~(1 << (n & 0x7));

  return 0;
} // pdip_cpu_unset


// ----------------------------------------------------------------------------
// Name   : pdip_cpu_isset
// Usage  : Check if the CPU number n is set in the bitmap
// Return : 1, if CPU number is set
//          0, if CPU number is not set
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int pdip_cpu_isset(
                   unsigned char *cpu,
                   unsigned int   n
                  )
{
  if (n >= pdip_nb_cpu)
  {
    errno = EINVAL;
    return -1;
  }

  if (cpu[n >> 3] & (1 << (n & 0x7)))
  {
    return 1;
  }

  return 0;
} // pdip_cpu_isset


// ----------------------------------------------------------------------------
// Name   : pdip_cpu_dup
// Usage  : Duplicate a bitmap of CPUs
// Return : bitmap, if OK
//          0, if error (errno is set)
// ----------------------------------------------------------------------------
unsigned char *pdip_cpu_dup(unsigned char *cpu_src)
{
unsigned char *cpu;

  if (!cpu_src)
  {
    errno = EINVAL;
    return 0;
  }

  cpu = (unsigned char *)malloc(PDIP_NB_BYTES_FOR_CPU() * sizeof(unsigned char *));

  if (cpu)
  {
  unsigned int i;

    for (i = 0; i < PDIP_NB_BYTES_FOR_CPU(); i ++)
    {
      cpu[i] = cpu_src[i];
    } // End for
  }

  return cpu;

} // pdip_cpu_dup


// ----------------------------------------------------------------------------
// Name   : pdip_get_user_cfg
// Usage  : Get the user configurable fields from the object descriptor
// Return : None
// ----------------------------------------------------------------------------
static void pdip_get_user_cfg(
                              pdip_ctx_t     *ctxp,
                              pdip_cfg_t     *cfg
                             )
{
  cfg->dbg_output           = ctxp->dbg_output;
  cfg->err_output           = ctxp->err_output;
  cfg->debug_level          = ctxp->debug;
  cfg->flags                = ctxp->flags;
  cfg->cpu                  = ctxp->cpu;
  cfg->buf_resize_increment = ctxp->buf_resize_increment;
} // pdip_get_user_cfg



// ----------------------------------------------------------------------------
// Name   : pdip_set_user_cfg
// Usage  : Set the user configurable fields in the object descriptor
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
static int pdip_set_user_cfg(
                              pdip_ctx_t     *ctxp,
                              pdip_cfg_t     *cfg
                            )
{
  if (cfg->dbg_output)
  {
    ctxp->dbg_output = cfg->dbg_output;
  }

  if (cfg->err_output)
  {
    ctxp->err_output = cfg->err_output;
  }

  ctxp->debug = cfg->debug_level;

  ctxp->flags = cfg->flags;

  if (cfg->cpu)
  {
  unsigned int i;

    // Duplicate the array to make sure that the user will not free
    // it after this call
    ctxp->cpu = pdip_cpu_alloc();
    if (!(ctxp->cpu))
    {
    int err_sav;

      err_sav = errno;
      PDIP_ERR(0, "malloc(%"PRISIZE"): '%m' (%d)\n", PDIP_NB_BYTES_FOR_CPU() * sizeof(unsigned char *), errno);
      errno = err_sav;
      return -1;
    }

    for (i = 0; i < PDIP_NB_BYTES_FOR_CPU(); i ++)
    {
      ctxp->cpu[i] = cfg->cpu[i];
    } // End for
  } // End if affinity

  // At least one slot for the terminating NUL
  if (cfg->buf_resize_increment > 1)
  {
    ctxp->buf_resize_increment = cfg->buf_resize_increment;
  }

  return 0;
} // pdip_set_user_cfg





// ----------------------------------------------------------------------------
// Name   : pdip_exec
// Usage  : Execute the process to be controlled
// Return : pid of the controlled program, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int pdip_exec(
              pdip_t  ctx,
              int     ac,
              char   *av[]
	     )
{
int             err_sav = 0;
int             rc;
char           *pty_slave_name;
int             fds;
pdip_ctx_t     *ctxp, child_ctx;
unsigned int    i;
struct termios  term_settings;
int             state;
pid_t           pid;
pdip_cfg_t      cfg;
unsigned int    saved_pdip_nb_cpu;

  if ((ac <= 0) || !av || !(av[0]) || !(av[ac - 1]) || (av[ac]) || !ctx)
  {
    errno = EINVAL;
    return -1;
  }

  ctxp = (pdip_ctx_t *)ctx;

  // Mutual exclusion with the signal handler
  PDIP_MASK_SIG();
  state = ctxp->state;
  PDIP_UNMASK_SIG();

  // If a program is already under control
  switch (state)
  {
    case PDIP_STATE_ALIVE: // A process is already attached to the object
    {
      PDIP_ERR(ctxp, "A process is already attached to the object\n");
      errno = EPERM;
      return -1;
    }
    break;

    case PDIP_STATE_DEAD: // Previous process attached to the context is dead
    {
      // Save user configurable fields
      pdip_get_user_cfg(ctxp, &cfg);

      // We want to attach a new process
      // Free the content of the object (but it is not unlinked)
      // and reinitialize the fields
      pdip_free_resources(ctxp);

      // Set the saved user configurable fields
      rc = pdip_set_user_cfg(ctxp, &cfg);
      if (0 != rc)
      {
        // errno is set
        return -1;
      }
    }
    break;

    case PDIP_STATE_INIT: // Context ready to exec
    {
    }
    break;

    case PDIP_STATE_ZOMBIE: // Process is dead but no pdip_status()/pdip_delete() done yet
    {
      PDIP_ERR(ctxp, "A dead process still attached to the object, pdip_status() or pdip_delete() is needed\n");
      errno = EPERM;
      return -1;
    }
    break;

    default : // Bad object, BUG ?!?
    {
      errno = EINVAL;
      return -1;
    }
    break;
  } // End switch

  // Populate the context with the program parameters
  ctxp->av = (char **)malloc((ac + 1) * sizeof(char *));
  if (!(ctxp->av))
  {
    // Errno is set
    return -1;
  }
  ctxp->ac = ac;
  for (i = 0; i < (unsigned)ac; i ++)
  {
    ctxp->av[i] = strdup(av[i]);
    if (!(ctxp->av[i]))
    {
      // Errno is set
      return -1;
    }
  } // End for
  ctxp->av[i] = (char *)0;

  // Get a master pty
  //
  // posix_openpt() opens a pseudo-terminal master and returns its file
  // descriptor.
  // It is equivalent to open("/dev/ptmx",O_RDWR|O_NOCTTY) on Linux systems :
  //
  //       . O_RDWR Open the device for both reading and writing
  //       . O_NOCTTY Do not make this device the controlling terminal for the process
  ctxp->pty_master = posix_openpt(O_RDWR |O_NOCTTY);
  if (ctxp->pty_master < 0)
  {
    err_sav = errno;
    PDIP_ERR(ctxp, "Impossible to get a master pseudo-terminal - errno = '%m' (%d)\n", errno);
    goto error;
  }

  // Grant access to the slave pseudo-terminal
  // (Chown the slave to the calling user)
  if (0 != grantpt(ctxp->pty_master))
  {
    err_sav = errno;
    PDIP_ERR(ctxp, "Impossible to grant access to slave pseudo-terminal - errno = '%m' (%d)\n", errno);
    goto error;
  }

  // Unlock pseudo-terminal master/slave pair
  // (Release an internal lock so the slave can be opened)
  if (0 != unlockpt(ctxp->pty_master))
  {
    err_sav = errno;
    PDIP_ERR(ctxp, "Impossible to unlock pseudo-terminal master/slave pair - errno = '%m' (%d)\n", errno);
    goto error;
  }

  // Get the name of the slave pty
  pty_slave_name = ptsname(ctxp->pty_master);
  if (NULL == pty_slave_name)
  {
    err_sav = errno;
    PDIP_ERR(ctxp, "Impossible to get the name of the slave pseudo-terminal - errno = '%m' (%d)\n", errno);
    goto error;
  }

  // Open the slave part of the terminal
  fds = open(pty_slave_name, O_RDWR);
  if (fds < 0)
  {
    err_sav = errno;
    PDIP_ERR(ctxp, "Impossible to open the slave pseudo-terminal - errno = '%m' (%d)\n", errno);
    goto error;
  }

  // To make "$" wildcard work, the end of lines must be Linux compatible (i.e. LF
  // instead of CR/LF). So, we disable mapping of LF to CR/LF on slave side.
  // We do it on master side (this impacts the whole PTY slave and master side) to
  // make sure that the user acting on master side will begin its interactions
  // after this configuration. If we do it on slave side, the user may send data
  // before the child configures the terminal
  rc = tcgetattr(ctxp->pty_master, &term_settings);
  if (rc != 0)
  {
    err_sav = errno;
    PDIP_ERR(ctxp, "tcgetattr(): '%m' (%d)\n", errno);
    errno = err_sav;
    exit(1);
  }
  if (ctxp->debug >= 20)
  {
    pdip_display_term_settings("master", &term_settings);
  }

  term_settings.c_oflag &= ~ONLCR;
  rc = tcsetattr(ctxp->pty_master, TCSANOW, &term_settings);
  if (rc != 0)
  {
    err_sav = errno;
    PDIP_ERR(ctxp, "tcsetattr(): '%m' (%d)\n", errno);
    errno = err_sav;
    exit(1);
  }

  // The pthread_atfork() routine of the library clears all the contexts in the
  // forked processes. So, we need to save the context in a local variable which
  // will be used by the child.
  // ==> Copy all the static fields and duplicate the dynamic ones
  //
  //     . av[] is not used from the context but from the parameters
  //     . cpu needs to be copied
  //     . pdip_nb_cpu needs to be copied
  child_ctx = *ctxp;
  if (ctxp->cpu)
  {
    child_ctx.cpu = pdip_cpu_dup(ctxp->cpu);
  }
  saved_pdip_nb_cpu = pdip_nb_cpu;

  // Fork a child
  pid = fork();

  // Update the pid field in the context as the signal handler
  // may be called if the process dies prematurely
  PDIP_MASK_SIG();
  ctxp->pid = child_ctx.pid = pid;
  PDIP_UNMASK_SIG();

  switch(pid)
  {
    case -1 :
    {
      err_sav = errno;
      PDIP_ERR(ctxp, "fork(): '%m' (%d)\n", errno);
      goto error;
    }
    break;

    case 0 : // Child
    {
    int fd;

      // Point on the copy of the context
      ctxp = &child_ctx;

      assert(fds > 2);
      pdip_assert(ctxp->pty_master > 2, "ctxp->pty_master=%d\n", ctxp->pty_master);

      // Redirect input/outputs to the slave side of PTY
      close(0);
      close(1);
      if (ctxp->flags & PDIP_FLAG_ERR_REDIRECT)
      {
        close(2);
      }
      fd = dup(fds);
      assert(0 == fd);
      fd = dup(fds);
      assert(1 == fd);
      if (ctxp->flags & PDIP_FLAG_ERR_REDIRECT)
      {
        fd = dup(fds);
        assert(2 == fd);
      }

      // Make some cleanups
      close(fds);
      close(ctxp->pty_master);

      // fds becomes the standard input
      fds = 0;

      // Make the child become a process session leader
      rc = setsid();
      if (rc < 0)
      {
        err_sav = errno;
        PDIP_ERR(ctxp, "setsid(): '%m' (%d)\n", errno);
        errno = err_sav;
        exit(1);
      }

      // As the child is a session leader, set the controlling terminal to be the slave
      // side of the PTY
      rc = ioctl(fds, TIOCSCTTY, 1);
      if (rc < 0)
      {
        err_sav = errno;
        PDIP_ERR(ctxp, "ioctl(TIOCSCTTY): '%m' (%d)\n", errno);
        errno = err_sav;
        exit(1);
      }

      // Make the foreground process group on the terminal be the process id of the child
      rc = tcsetpgrp(fds, getpid());
      if (rc < 0)
      {
        err_sav = errno;
        PDIP_ERR(ctxp, "setpgid(): '%m' (%d)\n", errno);
        errno = err_sav;
        exit(1);
      }

      rc = tcgetattr(fds, &term_settings);
      if (rc != 0)
      {
        err_sav = errno;
        PDIP_ERR(ctxp, "tcgetattr(): '%m' (%d)\n", errno);
        errno = err_sav;
        exit(1);
      }

      /*

        This perturbates the programs synchronizing on the output
        of the child as this output is not expected from the child process

      if (ctxp->debug >= 20)
      {
        pdip_display_term_settings("slave", &term_settings);
      }
      */

      // Set the CPU affinity if requested
      if (ctxp->cpu)
      {
      cpu_set_t mask;
      int       one = 0;

        CPU_ZERO(&mask);

        for (i = 0; i < saved_pdip_nb_cpu; i ++)
	{
          if (pdip_cpu_isset(ctxp->cpu, i))
	  {
            one = 1;
            CPU_SET(i, &mask);
	  } // End if CPU is set
	} // End for

        // If at least one bit is set otherwise we keep system defaults
        if (one)
	{
          rc = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
          if (0 != rc)
	  {
            err_sav = errno;
            PDIP_ERR(ctxp, "sched_setaffinity(): '%m' (%d)\n", errno);
            errno = err_sav;
            exit(1);
	  }
	}
      } // End if affinity

      // Exec the program (donc use av/ac from context as they are deallocated
      // upon fork() by the "atfork()" routine
      rc = execvp(av[0], av);

      // The error message can't be generated as the outputs are redirected to the PTY
      //PDIP_ERR(ctxp, "Error '%m' (%d) while running '%s'\n", errno, av[0]);

      _exit(1);
    }
    break;

    default: // Father
    {
      PDIP_DBG(ctxp, 1, "Forked process %"PRIPID" for program '%s'\n", ctxp->pid, av[0]);

      // Close the slave side of the PTY
      close(fds);

      // Be careful here : the child process may have died for some reasons
      // So, its state may not be INIT but DEAD

      // Update the context
      PDIP_MASK_SIG();
      if (PDIP_STATE_INIT != ctxp->state)
      {
        pdip_assert(PDIP_STATE_ZOMBIE == ctxp->state, "Unexpected state %d for process %"PRIPID"\n", ctxp->state, ctxp->pid);

        // Let the field as it is. We return OK to the user.
      }
      else
      {
        ctxp->state = PDIP_STATE_ALIVE;
      }
      PDIP_UNMASK_SIG();
    }
    break;
  } // End switch

  // If the signal handler detected the death of the process, this field
  // may be -1
  PDIP_MASK_SIG();
  pid = ctxp->pid;
  PDIP_UNMASK_SIG();

  if (-1 == pid)
  {
    // In this situation, errno is unknown
    errno = ECHILD;
  }
  return pid;

error:

  // Save user configurable fields
  pdip_get_user_cfg(ctxp, &cfg);

  // The context is not unlinked
  pdip_free_resources(ctxp);

  // Restore the user configurable resources
  (void)pdip_set_user_cfg(ctxp, &cfg);

  if (err_sav)
  {
    errno = err_sav;
  }

  return -1;

} // pdip_exec


// ----------------------------------------------------------------------------
// Name   : pdip_term_settings
// Usage  : Display the terminal settings
//          (this is not exported in header file, it for debug purposes)
// Return : None
// ----------------------------------------------------------------------------
void pdip_term_settings(pdip_t ctx)
{
struct termios  term_settings;
int             rc;
pdip_ctx_t     *ctxp;
int             err_sav;

  ctxp = (pdip_ctx_t *)ctx;

  rc = tcgetattr(ctxp->pty_master, &term_settings);
  if (rc != 0)
  {
    err_sav = errno;
    PDIP_ERR(ctxp, "tcgetattr(): '%m' (%d)\n", errno);
    errno = err_sav;
  }
  else
  {
    pdip_display_term_settings("master", &term_settings);
  }
} // pdip_term_settings


// ----------------------------------------------------------------------------
// Name   : pdip_flush
// Usage  : Flush the outstanding data
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int pdip_flush(
               pdip_t    ctx,
               char    **display,
               size_t   *display_sz,
               size_t   *data_sz
              )
{
pdip_ctx_t *ctxp;
int         state;

  if (!display || !display_sz || !data_sz || !ctx)
  {
    errno = EINVAL;
    return -1;
  }

  ctxp = (pdip_ctx_t *)ctx;

  // Mutual exclusion with the signal handler
  PDIP_MASK_SIG();
  state = ctxp->state;
  PDIP_UNMASK_SIG();

  if ((state != PDIP_STATE_ALIVE) && (state != PDIP_STATE_DEAD))
  {
    errno = EPERM;
    return -1;
  }

  return pdip_flush_internal(ctxp, display, display_sz, data_sz);

} // pdip_flush




// ----------------------------------------------------------------------------
// Name   : pdip_display_status
// Usage  : Display the exit status of a dead process
// ----------------------------------------------------------------------------
static void pdip_display_status(
                                pdip_ctx_t *ctxp
                               )
{
  if (WIFEXITED(ctxp->status))
  {
    PDIP_DBG(ctxp, 3, "Process '%s' (%"PRIPID") exited with status %d\n", ctxp->av[0], ctxp->pid, WEXITSTATUS(ctxp->status));
    return;
  }

  if (WIFSIGNALED(ctxp->status))
  {
    PDIP_DBG(ctxp, 3, "Process '%s' (%"PRIPID") received signal %d\n", ctxp->av[0], ctxp->pid, WTERMSIG(ctxp->status));
    return;
  }
} // pdip_display_status


//----------------------------------------------------------------------------
// Name        : pdip_update_dead_child
// Description : Update the context of a dead child
// Return      : None
//----------------------------------------------------------------------------
static void pdip_update_dead_child(
                                   pdip_ctx_t *ctxp,
                                   int         status
                                  )
{

  // Update the context
  PDIP_MASK_SIG();
  ctxp->status = status;
  ctxp->state  = PDIP_STATE_DEAD;
  ctxp->pid    = -1;
  PDIP_UNMASK_SIG();

  pdip_display_status(ctxp);

} // pdip_update_dead_child



//----------------------------------------------------------------------------
// Name        : pdip_wait_child
// Description : Wait for the termination of a child process (blocking)
// Return      : 0, if OK (process is dead)
//               -1, if error (errno is set)
//----------------------------------------------------------------------------
static int pdip_wait_child(
                           pdip_ctx_t *ctxp,
                           int        *status
                          )
{
int   rc;
int   state;
pid_t pid;

  PDIP_MASK_SIG();
  state = ctxp->state;
  *status = ctxp->status;
  pid = ctxp->pid;
  PDIP_UNMASK_SIG();

  // If the process is not marked dead, call the system to check
  if (PDIP_STATE_DEAD != state)
  {
    // When they dies, the childs interrupt waitpid()
    do
    {
      rc = waitpid(pid, status, 0);
    } while ((-1 == rc) && (EINTR == errno));

    // waitpid() returns the pid if the child is dead
    // otherwise it returns -1
    if (pid == rc)
    {
      // Child is dead
      rc = 0;

      // Update the context
      pdip_update_dead_child(ctxp, *status);
    }

    // If -1 == rc, child is not dead or error
  }
  else // Process marked dead
  {
    rc = 0;
  }

  // errno is set if rc is negative
  return rc;

} // pdip_wait_child


//----------------------------------------------------------------------------
// Name        : pdip_check_child
// Description : Check if a child process is terminated (not blocking)
// Return      : 1, if child is dead
//               0, if child is not dead
//               -1, if error (errno is set)
//----------------------------------------------------------------------------
static int pdip_check_child(
			    pdip_ctx_t *ctxp,
                            int        *status
                           )
{
int   rc;
int   state;
pid_t pid;

  PDIP_MASK_SIG();
  state   = ctxp->state;
  *status = ctxp->status;
  pid     = ctxp->pid;
  PDIP_UNMASK_SIG();

  // If the process is not marked dead, call the system to check
  if (PDIP_STATE_DEAD != state)
  {
    if (PDIP_STATE_INIT == state)
    {
      errno = EPERM;
      rc = -1;
    }
    else // Program attached to the object
    {
      // When they dies, the childs interrupt waitpid()
      do
      {
        rc = waitpid(pid, status, WNOHANG);
      } while ((-1 == rc) && (EINTR == errno));

      // waitpid(WNOHANG) returns 0 if the child is not dead yet
      // otherwise it returns -1
      if (pid == rc)
      {
        // Child is dead
        rc = 1;

        // Update the context
        pdip_update_dead_child(ctxp, *status);
      }

      // If 0 == rc, child is not dead
      // otherwise error

    } // End if no program attached to the object
  }
  else // Process marked dead
  {
    rc = 1;
  }

  // errno is set if rc is negative
  return rc;

} // pdip_check_child


// ----------------------------------------------------------------------------
// Name   : pdip_status
// Usage  : Return the status of the dead controlled program
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int pdip_status(
                pdip_t  ctx,
                int    *status,
                int     blocking
               )
{
int         obj_status;
pdip_ctx_t *ctxp;
int         rc;

  if (!ctx)
  {
    errno = EINVAL;
    return -1;
  }

  ctxp = (pdip_ctx_t *)ctx;

  if (blocking)
  {
    rc = pdip_wait_child(ctxp, &obj_status);
    if ((0 == rc) && status)
    {
      *status = obj_status;
    }
  }
  else // Non bloking mode
  {
    rc = pdip_check_child(ctxp, &obj_status);
    if (1 == rc)
    {
      if (status)
      {
        *status = obj_status;
      }

      rc = 0;
    }
    else if (0 == rc)
    {
      rc = -1;
      errno = EAGAIN;
    }
  } // End if blocking mode

  return rc;
} // pdip_status



// ----------------------------------------------------------------------------
// Name   : pdip_set_debug_level
// Usage  : Set the debug level for a given PDIP context (ctx != 0) or
//          set the global debug level
// Return : 0, if OK
//          !0, if error (errno is set)
// ----------------------------------------------------------------------------
int pdip_set_debug_level(
                         pdip_t ctx,
                         int    level
                        )
{
  // Mutual exclusion with the signal handler
  PDIP_MASK_SIG();

  if (!ctx)
  {
    pdip_debug_level = level;
  }
  else
  {
  pdip_ctx_t *ctxp;

    ctxp = (pdip_ctx_t *)ctx;
    ctxp->debug = level;
  }

  PDIP_UNMASK_SIG();

  return 0;
} // pdip_set_debug_level




// ----------------------------------------------------------------------------
// Name   : pdip_cfg_init
// Usage  : Initialize the configuration of a PDIP object to its default values
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int pdip_cfg_init(
                  pdip_cfg_t *cfg
                 )
{
  if (!cfg)
  {
    errno = EINVAL;
    return -1;
  }

  cfg->dbg_output           = (FILE *)0;
  cfg->err_output           = (FILE *)0;
  cfg->debug_level          = 0;
  cfg->flags                = 0;
  cfg->cpu                  = (unsigned char *)0;
  cfg->buf_resize_increment = 0;

  return 0;
} // pdip_cfg_init


// ----------------------------------------------------------------------------
// Name   : pdip_new
// Usage  : Allocate a PDIP context
// Return : PDIP object, if OK
//          0, if error (errno is set)
// ----------------------------------------------------------------------------
pdip_t pdip_new(
                pdip_cfg_t *cfg
               )
{
pdip_ctx_t *ctxp;

  // Allocate the context
  ctxp = (pdip_ctx_t *)malloc(sizeof(pdip_ctx_t));
  if (!ctxp)
  {
    PDIP_ERR(0, "malloc(%"PRISIZE"): '%m' (%d)\n", sizeof(pdip_ctx_t), errno);
    return (pdip_t)0;
  }

  // Populate the context
  pdip_init_ctx(ctxp);

  if (cfg)
  {
  int rc;

    rc = pdip_set_user_cfg(ctxp, cfg);
    if (0 != rc)
    {
    int err_sav;

      err_sav = errno;
      free(ctxp);
      errno = err_sav;
      return (pdip_t)0;
    }
  } // End if a configuration is passed

  ctxp->prev = 0;

  // Insert the object in the global list

  // Mutual exclusion with the signal handler
  PDIP_MASK_SIG();

  // Mutual exclusion with any other thread creating/deleting an object
  PDIP_LOCK();

  ctxp->next = pdip_ctx_list;
  if (pdip_ctx_list)
  {
    pdip_ctx_list->prev = ctxp;
  }
  pdip_ctx_list = ctxp;

  PDIP_UNLOCK();
  PDIP_UNMASK_SIG();

  return (pdip_t)ctxp;
} // pdip_new


//----------------------------------------------------------------------------
// Name        : pdip_kill_child
// Description : Send a signal safely to a child process
//----------------------------------------------------------------------------
static int pdip_kill_child(
                           pdip_ctx_t *ctxp,
                           int         sig
                          )
{
int rc;

  // Mask the SIGCHLD otherwise if the process dies, the signal handler will
  // set the pid to -1 in the context and so, we would send the signal to
  // all the processes!
  PDIP_MASK_SIG();
  if (ctxp->pid > 0)
  {
    PDIP_DBG(ctxp, 2, "Sending signal %d to '%s' (%"PRIPID")\n", sig, ctxp->av[0], ctxp->pid);
    rc = kill(ctxp->pid, sig);
  }
  else
  {
    errno = ENOENT;
    rc = -1;
  }
  PDIP_UNMASK_SIG();

  PDIP_DBG(ctxp, 4, "kill returned %d\n", rc);

  return rc;
} // pdip_kill_child






//----------------------------------------------------------------------------
// Name        : pdip_terminate_child
// Description : Terminate the child process
//----------------------------------------------------------------------------
static void pdip_terminate_child(
                                 pdip_ctx_t *ctxp,
                                 int        *status
                                )
{
int             obj_status;
pid_t           pid = ctxp->pid;
int             rc;

  // Mask the SIGCHLD otherwise if the process dies, the signal handler will
  // set the pid to -1 in the context
  (void)pdip_kill_child(ctxp, SIGTERM);

  // From here, the signal handler for SIGCHLD may be triggered
  // at any moment if the child dies

  // We check immediately as if we are lucky, the kill() system call triggered a
  // rescheduling giving a chance to the target process to die and report its status
  rc = pdip_check_child(ctxp, &obj_status);

  if (rc != 0)
  {
    PDIP_ERR(ctxp, "Checking for the termination of '%s' (%d) returned an error: '%m' (%d)...\n", ctxp->av[0], pid, errno);
  }

  // If the child is not dead
  if (1 != rc)
  {
  struct timespec timeout;

    if (rc != 0)
    {
      PDIP_ERR(ctxp, "Checking for the termination of '%s' (%d) returned an error: '%m' (%d)...\n", ctxp->av[0], pid, errno);
    }

    // Let's wait some ms before sending SIGKILL
    // nanosleep() may return prematurely if a signal is
    // received (e.g. SIGCHLD). In this case it returns -1 and sets errno to EINTR
    timeout.tv_sec = 0;
    timeout.tv_nsec = 25000000; // 25 ms
    rc = nanosleep(&timeout, 0);
    if (rc != 0)
    {
      PDIP_DBG(ctxp, 9, "nanosleep(): rc=%d, errno='%m' (%d)\n", rc, errno);
    }

    // Check the process status after timeout
    rc = pdip_check_child(ctxp, &obj_status);

    // If the sub-process didn't died ==> SIGKILL
    if (1 != rc)
    {
      (void)pdip_kill_child(ctxp, SIGKILL);

      PDIP_DBG(ctxp, 2, "Waiting for the termination of '%s' (%d)...\n", ctxp->av[0], pid);

      // Wait until the child is dead
      rc = pdip_wait_child(ctxp, &obj_status);

      if (rc != 0)
      {
        PDIP_ERR(ctxp, "Waiting for the termination of '%s' (%d) returned an error: '%m' (%d)...\n", ctxp->av[0], pid, errno);
      }

    } // End if sub-process didn't die
  } // End if sub-process didn't die

  PDIP_DBG(ctxp, 2, "Process '%s' (%d) is terminated with status %d\n", ctxp->av[0], pid, obj_status);

  *status = obj_status;

} // pdip_terminate_child




// ----------------------------------------------------------------------------
// Name   : pdip_sig
// Usage  : Send a signal to the controlled process
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int pdip_sig(
              pdip_t  ctx,
              int     sig
            )
{
pdip_ctx_t *ctxp;
int         state;

  if (!ctx)
  {
    errno = EINVAL;
    return -1;
  }

  ctxp = (pdip_ctx_t *)ctx;

  // Mutual exclusion with the signal handler
  PDIP_MASK_SIG();
  state = ctxp->state;
  PDIP_UNMASK_SIG();

  if (PDIP_STATE_ALIVE != state)
  {
    errno = EPERM;
    return -1;
  }

  return pdip_kill_child(ctxp, sig);
} // pdip_sig


// ----------------------------------------------------------------------------
// Name   : pdip_fd
// Usage  : Return the file descriptor of a PDIP object
// Return : File descriptor, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int pdip_fd(
            pdip_t  ctx
           )
{
pdip_ctx_t *ctxp;
int         state;

  if (!ctx)
  {
    errno = EINVAL;
    return -1;
  }

  ctxp = (pdip_ctx_t *)ctx;

  PDIP_MASK_SIG();
  state = ctxp->state;
  PDIP_UNMASK_SIG();

  if (PDIP_STATE_ALIVE != state)
  {
    PDIP_ERR(ctxp, "Controlled process is not alive\n");
    errno = EPERM;
    return -1;
  }

  return ctxp->pty_master;

} // pdip_fd



static void pdip_unlink_ctx(pdip_ctx_t *ctxp)
{

  PDIP_MASK_SIG();
  PDIP_LOCK();

  // Unlink the context
  if (ctxp->prev)
  {
    ctxp->prev->next = ctxp->next;
  }
  if (ctxp->next)
  {
    ctxp->next->prev = ctxp->prev;
  }
  if (pdip_ctx_list == ctxp)
  {
    pdip_ctx_list = ctxp->next;
  }

  PDIP_UNLOCK();
  PDIP_UNMASK_SIG();

} // pdip_unlink_ctx


// ----------------------------------------------------------------------------
// Name   : pdip_delete
// Usage  : Deallocate a PDIP context
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int pdip_delete(
                pdip_t  ctx,
                int    *status
               )
{
pdip_ctx_t *ctxp;
pdip_ctx_t *p;
int         state;
int         obj_status;

  if (!ctx)
  {
    errno = EINVAL;
    return -1;
  }

  ctxp = (pdip_ctx_t *)ctx;

  PDIP_MASK_SIG();
  PDIP_LOCK();

  // Look for the context in the list
  p = pdip_ctx_list;
  while (p)
  {
    if (ctxp == p)
    {
      break;
    }
    p = p->next;
  } // End while

  PDIP_UNLOCK();

  // As "ctxp->state" may be modified by the signal handler, we mask SIGCHLD
  // to access it atomically
  if (p)
  {
    state = ctxp->state;
    obj_status = ctxp->status;
  }

  PDIP_UNMASK_SIG();

  // If context not found
  if (!p)
  {
    errno = ENOENT;
    return -1;
  }

  // If the controlled program is not finished
  switch (state)
  {
    case PDIP_STATE_ALIVE:
    {
      // Terminate it (with update of obj_status)
      pdip_terminate_child(ctxp, &obj_status);
    }
    break;

    case PDIP_STATE_DEAD:
    {
      // Controlled program is dead
      // obj_status is already read
    }
    break;

    case PDIP_STATE_ZOMBIE:
    {
      // Controlled program is dead
      // obj_status is not read yet
      if (0 != pdip_wait_child(ctxp, &obj_status))
      {
        PDIP_ERR(ctxp, "Error while getting process status: '%m' (%d)\n", errno);
        // errno is set
        return -1;
      }
    }
    break;

    case PDIP_STATE_INIT:
    {
      // Object did not execute anything ?
      // obj_status is already read
    }
    break;

    default: // Bug ?!?
    {
      pdip_assert(0, "Unexpected state %d\n", state);
    }
    break;
  } // End if switch state

  // Unlink the context
  pdip_unlink_ctx(p);

  pdip_free_resources(ctxp);

  // The previous call does not touch the links
  // ==> Reset them
  ctxp->prev = ctxp->next = (pdip_ctx_t *)0;

  free(ctxp);

  // Return the process status if requested
  if (status)
  {
    *status = obj_status;
  }

  return 0;
} // pdip_delete



// ----------------------------------------------------------------------------
// Name   : pdip_signal_handler
// Usage  : Signal handler
// Return : PDIP_SIG_HANDLED, if the signal has been handled by the service
//          PDIP_SIG_UNKNOWN, if the signal has not been handled by the service
//          PDIP_SIG_ERROR, if error
// ----------------------------------------------------------------------------
int pdip_signal_handler(
                        int        sig,   // Received signal
                        siginfo_t *info   // Context of the signal
                       )
{
  if (!info)
  {
    PDIP_ERR(0, "Signal handler called with an empty context for signal %d\n", sig);
    return PDIP_SIG_ERROR;
  }

  switch(sig)
  {
    case SIGCHLD:
    {
    pdip_ctx_t *ctxp;

      // A controlled program may be dead

      // Look for the context to check that it is a controlled program
      // We use the lock because an alternate thread may run concurrently
      // on another processor to create/delete objects
      PDIP_LOCK();
      ctxp = pdip_ctx_list;
      while (ctxp)
      {
        if (info->si_pid == ctxp->pid)
	{
          PDIP_DBG(ctxp, 2, "Catched SIGCHLD from process '%s' (%"PRIPID"), state=%d\n", ctxp->av[0], ctxp->pid, ctxp->state);
          break;
	}

        ctxp = ctxp->next;
      } // End while
      PDIP_UNLOCK();

      if (ctxp)
      {
        // A process may die immediately after execution and
        // consequently, the state may not be updated yet by the father
        if (PDIP_STATE_ALIVE != ctxp->state)
	{
          pdip_assert(PDIP_STATE_INIT == ctxp->state, "Unexpected state %d for process %"PRIPID"\n", ctxp->state, ctxp->pid);
          PDIP_DBG(ctxp, 1, "Process %"PRIPID" died prematurely!\n", ctxp->pid);
	}

        ctxp->state = PDIP_STATE_ZOMBIE;

        // The status and reset of pid will be done by a subsequente pdip_status() or pdip_delete()

        PDIP_DBG(ctxp, 1, "ctxp=%p, State=%d\n", ctxp, ctxp->state);

        return PDIP_SIG_HANDLED;
      }
      else
      {
        PDIP_DBG(0, 3, "Catched SIGCHLD from process %d which is not linked to any PDIP object\n", info->si_pid);
        return PDIP_SIG_ERROR;
      }
    }
    break;

    default:
    {
      PDIP_ERR(0, "Unexpected signal#%d from process %d\n", sig, info->si_pid);
      return PDIP_SIG_UNKNOWN;
    }
  } // End switch

} // pdip_signal_handler


// ----------------------------------------------------------------------------
// Name   : pdip_internal_sig_hdl
// Usage  : Signal handler
// ----------------------------------------------------------------------------
static void pdip_internal_sig_hdl(
                                  int        sig,
                                  siginfo_t *info,
                                  void      *p
                                 )
{
int rc;

  (void)p;

  rc = pdip_signal_handler(sig, info);

  // A process linked with PDIP may call fork() at any time. Hence, a SIGCHLD
  // may occur at any time but it is not linked to any PDIP context as the
  // child processes are not the results of calls to pdip_exec().
  if (PDIP_SIG_HANDLED != rc)
  {
    PDIP_DBG(0, 3, "Signal not handled: rc=%d, sig=%d\n", rc, sig);
  }
} // pdip_internal_sig_hdl




// ----------------------------------------------------------------------------
// Name   : pdip_sig_hdl_internal
// Usage  : Type of signal handler (internal/external)
// ----------------------------------------------------------------------------
static int pdip_sig_hdl_internal;


// ----------------------------------------------------------------------------
// Name   : pdip_configure
// Usage  : Configuration of PDIP
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int pdip_configure(
                    int sig_hdl_internal,
                    int debug_level
                  )
{
  PDIP_LOCK();
  PDIP_MASK_SIG();
  pdip_debug_level = debug_level;
  PDIP_UNMASK_SIG();
  PDIP_UNLOCK();

  if (sig_hdl_internal)
  {
  int              rc;
  int              err_sav;
  struct sigaction action;

    // Register the signal handler
    action.sa_sigaction = pdip_internal_sig_hdl;
    action.sa_mask      = pdip_sigset;
    action.sa_flags     = SA_SIGINFO;
    rc = sigaction(SIGCHLD, &action, &pdip_saved_action);
    if (rc < 0)
    {
      err_sav = errno;
      PDIP_ERR(0, "sigaction(SIGCHLD): '%m' (%d)\n", errno);
      errno = err_sav;
      goto err;
    }

    pdip_sig_hdl_internal = 1;
  }
  else
  {
    // The user is supposed to call pdip_signal_handler() from its signal
    // handler for SIGCHLD
  }

  return 0;

err:

  return -1;
} // pdip_configure


// ----------------------------------------------------------------------------
// Name   : pdip_child_fork
// Usage  : Clear all the contexts of the library without killing the
//          controlled processes upon fork()
// ----------------------------------------------------------------------------
static void pdip_child_fork(void)
{
pdip_ctx_t *ctxp;
int         rc;

  PDIP_DBG(0, 5, "PDIP forked from process %d!\n", getppid());

  if (pdip_sig_hdl_internal)
  {
    // The signal handler is inherited from the father
    // ==> Cancel it to the old value
    rc = sigaction(SIGCHLD, &pdip_saved_action, 0);
    if (rc < 0)
    {
      PDIP_ERR(0, "sigaction(SIGCHLD): '%m' (%d)\n", errno);
    }
  }

  while (pdip_ctx_list)
  {
    ctxp = pdip_ctx_list;

    // Free the resources et reinitialize the context
    (void)pdip_free_resources(ctxp);

    // Unlink the context
    pdip_unlink_ctx(ctxp);

    // For debug purposes before freeing
    ctxp->prev = ctxp->next = (pdip_ctx_t *)0;

    // Free the context
    free(ctxp);

  } // End while

  pdip_nb_cpu = 0;

  (void)sigemptyset(&pdip_sigset);

  rc = pthread_mutex_destroy(&pdip_mtx);
  if (rc != 0)
  {
    errno = rc;
    PDIP_ERR(0, "pthread_mutex_destroy(): '%m' (%d)\n", errno);
  }

  pdip_debug_level = 0;

  // If one still needs the PDIP service in a child process,
  // pdip_lib_initialize() must be called explicitely

} // pdip_child_fork


// ----------------------------------------------------------------------------
// Name   : pdip_lib_initialize
// Usage  : Library initialization when needed in a child process
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
int pdip_lib_initialize(void)
{
int rc;

  // Initialize the mutex
  rc = pthread_mutex_init(&pdip_mtx, NULL);
  if (rc != 0)
  {
    errno = rc;
    PDIP_ERR(0, "pthread_mutex_init(): '%m' (%d)\n", errno);
    return -1;
  }

  // Get the number of CPUs
  rc = sysconf(_SC_NPROCESSORS_ONLN);
  if (rc < 0)
  {
    PDIP_ERR(0, "sysconf(): '%m' (%d)\n", errno);
    return -1;
  }

  assert(rc > 0);
  pdip_nb_cpu = rc;

  // Set the signal mask
  (void)sigemptyset(&pdip_sigset);
  (void)sigaddset(&pdip_sigset, SIGCHLD);

  rc = pthread_atfork(0, 0, pdip_child_fork);
  if (0 != rc)
  {
    errno = rc;
    PDIP_ERR(0, "pthread_atfork(): '%m' (%d)\n", errno);
    return -1;
  }

  return 0;
} // pdip_lib_initialize


static void __attribute__ ((constructor)) pdip_lib_init(void);

// ----------------------------------------------------------------------------
// Name   : pdip_lib_init
// Usage  : Library entry point
// ----------------------------------------------------------------------------
static void pdip_lib_init(void)
{

  (void)pdip_lib_initialize();

} // pdip_lib_init



static void __attribute__ ((destructor)) pdip_lib_exit(void);

// ----------------------------------------------------------------------------
// Name   : pdip_lib_exit
// Usage  : Exit point of the library
// ----------------------------------------------------------------------------
static void pdip_lib_exit(void)
{
  PDIP_DBG(0, 3, "PDIP lib exiting...\n");

  // Free the remaining objects
  while (pdip_ctx_list)
  {
    // Delete the head of the list
    (void)pdip_delete(pdip_ctx_list, 0);
  } // End while

  (void)pthread_mutex_destroy(&pdip_mtx);

} // pdip_lib_exit


