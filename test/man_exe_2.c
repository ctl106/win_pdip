// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : man_exe_2.c
// Description : Programmed Dialogue with Interactive Programs
//               Example for "man 3 pdip": Piloting "bc" tool
// License     :
//
//  Copyright (C) 2007-2018 Rachid Koucha <rachid dot koucha at gmail dot com>
//
//  This program is free and destined to help people using PDIP library
//
//
// Evolutions  :
//
//     28-Nov-2017  R. Koucha    - Creation
//     26-Dec-2018  R. Koucha    - Added call to pdip_cfg_init()
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include "pdip.h"





int main(int ac, char *av[])
{
pdip_t      pdip;
char       *bash_av[3];
int         rc;
char       *display;
size_t      display_sz;
size_t      data_sz;
pdip_cfg_t  cfg;
char       *op;
int         i;
int         status;

  if (ac != 2)
  {
    fprintf(stderr, "Usage: %s operation\n", basename(av[0]));
    return 1;
  }

  // Let the service manage the SIGCHLD signal as we don't fork/exec any
  // other program
  rc = pdip_configure(1, 0);
  if (rc != 0)
  {
    fprintf(stderr, "pdip_configure(): '%m' (%d)\n", errno);
    return 1;
  }

  op = av[1];

  // Create a PDIP object
  pdip_cfg_init(&cfg);
  cfg.debug_level = 0;
  pdip = pdip_new(&cfg);
  if (!pdip)
  {
    fprintf(stderr, "pdip_new(): '%m' (%d)\n", errno);
    return 1;
  }

  //pdip_set_debug_level(pdip, 5);

  // Attach the "bc" command to the PDIP object
  // Option "-q" launches "bc" in quiet mode: it does not display
  // the welcome banner
  bash_av[0] = "bc";
  bash_av[1] = "-q";
  bash_av[2] = (char *)0;
  rc = pdip_exec(pdip, 2, bash_av);
  if (rc < 0)
  {
    fprintf(stderr, "pdip_exec(bc -q): '%m' (%d)\n", errno);
    return 1;
  }

  // Execute the operation
  rc = pdip_send(pdip, "%s\n", op);
  if (rc < 0)
  {
    fprintf(stderr, "pdip_send(op): '%m' (%d)\n", errno);
    return 1;
  }

  // Initialize the display buffer
  display = (char *)0;
  display_sz = 0;
  data_sz = 0;

  // For some reasons, "bc" echoes the operation two times ?!?
  // ==> Skip them
  for (i = 0; i < 2; i ++)
  {
    // Synchronize on the echo
    // We pass the same buffer that will be eventually reallocated
    rc = pdip_recv(pdip, "^.+$", &display, &display_sz, &data_sz, (struct timeval*)0);
    if (rc != PDIP_RECV_FOUND)
    {
      fprintf(stderr, "pdip_recv(): Unexpected return code %d\n", rc);
      return 1;
    }

    // Print the operation on the screen (one time :-)
    if (0 == i)
    {
      printf("%s=", display);
    }

    // Skip the end of line
    rc = pdip_recv(pdip, "$", &display, &display_sz, &data_sz, (struct timeval*)0);
    if (rc != PDIP_RECV_FOUND)
    {
      fprintf(stderr, "pdip_recv($): Unexpected return code %d\n", rc);
      return 1;
    }
  } // End for

  // Synchronize on the result of the operation
  rc = pdip_recv(pdip, "^.+$", &display, &display_sz, &data_sz, (struct timeval*)0);
  if (rc != PDIP_RECV_FOUND)
  {
    fprintf(stderr, "pdip_recv(): Unexpected return code %d\n", rc);
    return 1;
  }

  // Display the result of the operation with '\n' as the match
  // does not embed the end of line
  printf("%s\n", display);
  fflush(stdout);

  // Skip the end of line
  rc = pdip_recv(pdip, "$", &display, &display_sz, &data_sz, (struct timeval*)0);
  if (rc != PDIP_RECV_FOUND)
  {
    fprintf(stderr, "pdip_recv($): Unexpected return code %d\n", rc);
    return 1;
  }

  // Execute "quit" to go out
  rc = pdip_send(pdip, "quit\n");
  if (rc < 0)
  {
    fprintf(stderr, "pdip_send(quit): '%m' (%d)\n", errno);
    return 1;
  }

  // Synchronize on the echo of "quit"
  // We pass the same buffer that will be eventually reallocated
  rc = pdip_recv(pdip, "^quit$", &display, &display_sz, &data_sz, (struct timeval*)0);
  if (rc != PDIP_RECV_FOUND)
  {
    fprintf(stderr, "pdip_recv(): Unexpected return code %d\n", rc);
    return 1;
  }

  // Wait for the end of "bc"
  rc = pdip_status(pdip, &status, 1);
  if (0 != rc)
  {
    fprintf(stderr, "pdip_status(): '%m' (%d)\n", errno);
    return 1;
  }

  printf("bc ended with status 0x%x\n", status);

  // Delete the PDIP object
  rc = pdip_delete(pdip, 0);
  if (rc != 0)
  {
    fprintf(stderr, "pdip_delete(): '%m' (%d)\n", errno);
    return 1;
  }

  return 0;

} // main
