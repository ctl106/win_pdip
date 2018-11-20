// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : tpdip.c
// Description : Test for Programmed Dialogue with Interactive Programs
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
//     06-Nov-2017  R. Koucha    - Creation
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=



#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>

#include "pdip.h"
#include "pdip_util.h"










static void sig_hdl(int sig, siginfo_t *info, void *p)
{
int rc;

 (void)p;

  rc = pdip_signal_handler(sig, info);
  switch(rc)
  {
    case PDIP_SIG_UNKNOWN:
    {
      printf("PDIP signal handler returned PDIP_SIG_UNKNOWN\n");
    }
    break;
    case PDIP_SIG_HANDLED:
    {
      printf("PDIP signal handler returned PDIP_SIG_HANDLED\n");
    }
    break;
    default:
    {
      printf("PDIP signal handler returned %d\n", rc);
    }
  }

} // sig_hdl





int main(
         int ac,
         char *av[]
	)
{
int             rc;
pdip_t          ctx;
char           *display;
size_t          display_sz;
size_t          data_sz;
struct timeval  timeout;
pdip_cfg_t      cfg;
struct sigaction  action;
sigset_t          sigset;

  if (ac < 2)
  {
    fprintf(stderr, "Usage: %s program params...\n", basename(av[0]));
    return 1;
  }

  // Register the signal handler
  (void)sigemptyset(&sigset);
  (void)sigaddset(&sigset, SIGCHLD);
  action.sa_sigaction = sig_hdl;
  action.sa_mask      = sigset;
  action.sa_flags     = SA_SIGINFO;
  rc = sigaction(SIGCHLD, &action, 0);
  if (rc < 0)
  {
    fprintf(stderr, "sigaction(): '%m' (%d)\n", errno);
  }
  

  rc = pdip_configure(0, 0);
  if (rc != 0)
  {
    fprintf(stderr, "pdip_configure(): '%m' (%d)\n", errno);
  }


  printf("\n============================= WITHOUT REGEX =====================\n");

  pdip_cfg_init(&cfg);
  cfg.dbg_output = (FILE *)0;
  cfg.err_output = (FILE *)0;
  cfg.debug_level = 0;
  ctx = pdip_new(&cfg);
  if (!ctx)
  {
    return 1;
  }

  rc = pdip_set_debug_level(ctx, 9);
  if (rc != 0)
  {
    fprintf(stderr, "pdip_set_debug_level(): '%m' (%d)\n", errno);
  }


  rc = pdip_exec(ctx, ac - 1, &(av[1]));
  if (rc != 0)
  {
    fprintf(stderr, "pdip_exec(): '%m' (%d)\n", errno);
  }

  printf("Controlling '%s'...\n", av[1]);

  display = 0;
  display_sz = 0;
  do
  {
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    rc = pdip_recv(ctx, 0, &display, &display_sz, &data_sz, &timeout);
    if (PDIP_RECV_DATA == rc)
    {
      printf("Received data (%zu bytes in buffer of %zu bytes) : '%s'\n", data_sz, display_sz, display);
      pdip_dump(display, data_sz);
    }
  } while (PDIP_RECV_DATA == rc);

  if (PDIP_RECV_TIMEOUT == rc)
  {
    printf("Timeout (rc=%d), data_sz=%zu\n", rc, data_sz);
    if (data_sz > 0)
    {
      printf("Received data (%zu bytes in buffer of %zu bytes) : '%s'\n", data_sz, display_sz, display);
      pdip_dump(display, data_sz);
    }
  }
  else
  {
    printf("Error, rc=%d\n", rc);
  }

  rc = pdip_send(ctx, "ls --color=auto\n");
  if (rc < 0)
  {
    fprintf(stderr, "pdip_send(ls): '%m' (%d)\n", errno);
  }

  timeout.tv_sec = 3;
  timeout.tv_usec = 0;
  do
  {
    rc = pdip_recv(ctx, 0, &display, &display_sz, &data_sz, &timeout);
    if (PDIP_RECV_DATA == rc)
    {
      printf("Received data (%zu bytes in buffer of %zu bytes) : '%s'\n", data_sz, display_sz, display);
      pdip_dump(display, data_sz);
    }
  } while (PDIP_RECV_DATA == rc);


  if (PDIP_RECV_TIMEOUT == rc)
  {
    printf("Timeout (rc=%d), data_sz=%zu\n", rc, data_sz);
    if (data_sz > 0)
    {
      printf("Received data (%zu bytes in buffer of %zu bytes) : '%s'\n", data_sz, display_sz, display);
      pdip_dump(display, data_sz);
    }
  }
  else
  {
    printf("Error, rc=%d\n", rc);
  }

  rc = pdip_send(ctx, "exit\n");
  if (rc < 0)
  {
    fprintf(stderr, "pdip_send(ls): '%m' (%d)\n", errno);
  }

  // SIGCHLD must be received as exit will terminate the shell
  pause();

  rc = pdip_recv(ctx, 0, &display, &display_sz, &data_sz, 0);
  printf("rc=%d, data_sz=%zu\n", rc, data_sz);
  if (rc < 0)
  {
    fprintf(stderr, "pdip_recv(): '%m' (%d)\n", errno);
  }
  if (data_sz > 0)
  {
    printf("Received data (%zu bytes in buffer of %zu bytes) : '%s'\n", data_sz, display_sz, display);
  }

  // Bad regex
  rc = pdip_recv(ctx, "(badreg", &display, &display_sz, &data_sz, 0);
  printf("rc=%d, data_sz=%zu\n", rc, data_sz);

  rc = pdip_delete(ctx, 0);
  if (rc < 0)
  {
    fprintf(stderr, "pdip_delete(): '%m' (%d)\n", errno);
  }



  printf("\n============================= WITH REGEX =====================\n");


  ctx = pdip_new(&cfg);
  if (!ctx)
  {
    return 1;
  }

  rc = pdip_set_debug_level(ctx, 9);
  if (rc != 0)
  {
    fprintf(stderr, "pdip_set_debug_level(): '%m' (%d)\n", errno);
  }

  rc = pdip_exec(ctx, ac - 1, &(av[1]));
  if (rc != 0)
  {
    fprintf(stderr, "pdip_exec(): '%m' (%d)\n", errno);
  }

  printf("Controlling '%s'...\n", av[1]);
  
  display = 0;
  display_sz = 0;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  rc = pdip_recv(ctx, "/DEVS/PDIP\\$ $", &display, &display_sz, &data_sz, &timeout);
  printf("rc = %d, display_sz=%zu, data_sz=%zu\n%s\n", rc, display_sz, data_sz, display);
  
  rc = pdip_send(ctx, "ls --color=auto\n");
  if (rc < 0)
  {
    fprintf(stderr, "pdip_send(ls): '%m' (%d)\n", errno);
  }

  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  rc = pdip_recv(ctx, "pdip_util.c$", &display, &display_sz, &data_sz, &timeout);
  printf("rc = %d, display_sz=%zu, data_sz=%zu\n%s\n", rc, display_sz, data_sz, display);
  
  rc = pdip_delete(ctx, 0);
  if (rc < 0)
  {
    fprintf(stderr, "pdip_delete(): '%m' (%d)\n", errno);
  }

  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  rc = pdip_recv(ctx, "/DEVS/PDIP\\$ $", &display, &display_sz, &data_sz, &timeout);
  printf("rc = %d, display_sz=%zu, data_sz=%zu\n%s\n", rc, display_sz, data_sz, display);
  
  rc = pdip_delete(ctx, 0);
  if (rc < 0)
  {
    fprintf(stderr, "pdip_delete(): '%m' (%d)\n", errno);
  }


  return 0;
} // main
