// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : man_exe_1.c
// Description : Programmed Dialogue with Interactive Programs
//               Example for "man 3 pdip": piloting "bash" shell
// License     :
//
//  Copyright (C) 2007-2018 Rachid Koucha <rachid dot koucha at gmail dot com>
//
//
//  This program is free and destined to help people using PDIP library
//
//
// Evolutions  :
//
//     24-Nov-2017  R. Koucha    - Creation
//     26-Dec-2018  R. Koucha    - Added call to pdip_cfg_init()
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "pdip.h"





int main(int ac, char *av[])
{
pdip_t      pdip;
char       *bash_av[4];
int         rc;
char       *display;
size_t      display_sz;
size_t      data_sz;
pdip_cfg_t  cfg;
int         status;

  (void)ac;
  (void)av;

  // Let the service manage the SIGCHLD signal as we don't fork/exec any
  // other program
  rc = pdip_configure(1, 0);
  if (rc != 0)
  {
    fprintf(stderr, "pdip_configure(): '%m' (%d)\n", errno);
    return 1;
  }

  // Create a PDIP object
  pdip_cfg_init(&cfg);
  // The bash prompt is displayed on stderr. So, to synchronize on it, we must
  // redirect stderr to the PTY between PDIP and bash
  cfg.flags |= PDIP_FLAG_ERR_REDIRECT;
  cfg.debug_level = 0;
  pdip = pdip_new(&cfg);
  if (!pdip)
  {
    fprintf(stderr, "pdip_new(): '%m' (%d)\n", errno);
    return 1;
  }

  //pdip_set_debug_level(pdip, 20);

  // Export the prompt of the BASH shell
  rc = setenv("PS1", "PROMPT> ", 1);
  if (rc != 0)
  {
    fprintf(stderr, "setenv(PS1): '%m' (%d)\n", errno);
    return 1;
  }

  // Attach a bash shell to the PDIP object
  bash_av[0] = "/bin/bash";
  bash_av[1] = "--noprofile";
  bash_av[2] = "--norc";
  bash_av[3] = (char *)0;
  rc = pdip_exec(pdip, 3, bash_av);
  if (rc < 0)
  {
    fprintf(stderr, "pdip_exec(bash): '%m' (%d)\n", errno);
    return 1;
  }

  // Synchronize on the first displayed prompt
  display = (char *)0;
  display_sz = 0;
  data_sz = 0;
  rc = pdip_recv(pdip, "^PROMPT> ", &display, &display_sz, &data_sz, (struct timeval*)0);
  if (rc != PDIP_RECV_FOUND)
  {
    fprintf(stderr, "pdip_recv(): Unexpected return code %d\n", rc);
    return 1;
  }

  // Display the result
  printf("%s", display);

  // Execute the "ls -la /" command
  rc = pdip_send(pdip, "ls -la /\n");
  if (rc < 0)
  {
    fprintf(stderr, "pdip_send(ls -la /): '%m' (%d)\n", errno);
    return 1;
  }

  // Synchronize on the prompt displayed right after the command execution
  // We pass the same buffer that will be eventually reallocated
  rc = pdip_recv(pdip, "^PROMPT> ", &display, &display_sz, &data_sz, (struct timeval*)0);
  if (rc != PDIP_RECV_FOUND)
  {
    fprintf(stderr, "pdip_recv(): Unexpected return code %d\n", rc);
    return 1;
  }

  // Display the result
  printf("%s", display);

  // Execute "exit" to go out of the shell
  rc = pdip_send(pdip, "exit\n");
  if (rc < 0)
  {
    fprintf(stderr, "pdip_send(exit): '%m' (%d)\n", errno);
    return 1;
  }

  // Wait for the end of "bash"
  rc = pdip_status(pdip, &status, 1);
  if (0 != rc)
  {
    fprintf(stderr, "pdip_status(): '%m' (%d)\n", errno);
    return 1;
  }

  printf("bash ended with status 0x%x\n", status);

  // Delete the PDIP object
  rc = pdip_delete(pdip, 0);
  if (rc != 0)
  {
    fprintf(stderr, "pdip_delete(): '%m' (%d)\n", errno);
    return 1;
  }

  return 0;

} // main
