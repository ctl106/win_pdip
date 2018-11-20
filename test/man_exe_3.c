// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : man_exe_3.c
// Description : Programmed Dialogue with Interactive Programs
//               Example for "man 3 pdip_cpu"
// License     :
//
//  Copyright (C) 2007-2018 Rachid Koucha <rachid dot koucha at gmail dot com>
//
//  This program is free and destined to help people using PDIP library
//
//
// Evolutions  :
//
//     27-Dec-2017  R. Koucha    - Creation
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include "pdip.h"





int main(int ac, char *av[])
{
pdip_t      pdip;
char       *new_av[5];
int         rc;
char       *display;
size_t      display_sz;
size_t      data_sz;
pdip_cfg_t  cfg;
int         status;
char        reg_expr[256];
int         pid_prog;
char       *processor;

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

  // Create a PDIP object and make it run the program
  // on the last CPU
  pdip_cfg_init(&cfg);
  cfg.debug_level = 0;
  cfg.cpu = pdip_cpu_alloc();

  // Choose first and last CPU for the program
  pdip_cpu_set(cfg.cpu, 0);
  pdip_cpu_set(cfg.cpu, pdip_cpu_nb() - 1);
  pdip = pdip_new(&cfg);
  if (!pdip)
  {
    fprintf(stderr, "pdip_new(): '%m' (%d)\n", errno);
    return 1;
  }

  //pdip_set_debug_level(pdip, 4);

  // Attach the "ps " command to the PDIP object
  new_av[0] = "ps";
  new_av[1] = "-e";
  new_av[2] = "-o";
  new_av[3] = "pid,ppid,psr,comm";
  new_av[4] = (char *)0;
  pid_prog = pdip_exec(pdip, 4, new_av);
  if (pid_prog < 0)
  {
    fprintf(stderr, "pdip_exec(ps): '%m' (%d)\n", errno);
    return 1;
  }

  // Make the regular expression to catch "ps" process in the result
  // of the command
  snprintf(reg_expr, sizeof(reg_expr), "%d([ ])+%d([ ])+([0-9])+([ ])+ps", pid_prog, getpid());

  // Initialize the display buffer
  display = (char *)0;
  display_sz = 0;
  data_sz = 0;

  // Receive data
  rc = pdip_recv(pdip, reg_expr, &display, &display_sz, &data_sz, 0);
  if (rc != PDIP_RECV_FOUND)
  {
    fprintf(stderr, "Regular expression '%s' not found\n", reg_expr);
    return 1;
  }

  // Get processor number in the output of 'ps' command
  processor = display + data_sz; // End of buffer
  while (' ' != *processor) // Skip command name
  {
    processor --;
  }
  while (' ' == *processor) // Skip spaces between command and processor number
  {
    processor --;
  }
  *(processor + 1) = '\0'; // NUL terminate the processor number string
  while (' ' != *processor) // Go to the beginning of the processor number string
  {
    processor --;
  }
  processor ++;
  printf("'ps' runs on processor#%s\n", processor);

  // Wait for the end of "ps"
  rc = pdip_status(pdip, &status, 1);
  if (0 != rc)
  {
    fprintf(stderr, "pdip_status(): '%m' (%d)\n", errno);
    return 1;
  }

  // Delete the PDIP object
  rc = pdip_delete(pdip, 0);
  if (rc != 0)
  {
    fprintf(stderr, "pdip_delete(): '%m' (%d)\n", errno);
    return 1;
  }

  // Delete the CPU bitmap
  pdip_cpu_free(cfg.cpu);

  return 0;

} // main
