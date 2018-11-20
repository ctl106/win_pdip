// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : tisys.c
// Description : Test isys library 
//
// License     :
//
//  Copyright (C) 2007-2018 Rachid Koucha <rachid dot koucha at gmail dot com>
//
//
//  This program is free and can be used and modified by anybody wanting to
//  to use the ISYS library.
//
//
// Evolutions  :
//
//     20-Dec-2017  R. Koucha    - Creation
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include "isys/isys.h"


int main(int ac, char *av[])
{
int     status;
int     i;
char   *cmdline;
size_t  len;
size_t  offset;

  if (ac < 2)
  {
    fprintf(stderr, "Usage: %s cmd params...\n", basename(av[0]));
    return 1;
  }

  // Build the command line
  cmdline = (char *)0;
  len     = 1; // Terminating NUL
  offset = 0;
  for (i = 1; i < ac; i ++)
  {
    len += strlen(av[i]) + 1; // word + space
    cmdline = (char *)realloc(cmdline, len);
    assert(cmdline);
    offset += sprintf(cmdline + offset, "%s ", av[i]);
  } // End for

  printf("Running '%s'...\n", cmdline);

  status = isystem("%s", cmdline);
  if (status != 0)
  {
    printf("Error from program (0x%x)!\n", status);
    free(cmdline);
    return  1;
  } // End if

  free(cmdline);
  return 0;
} // main
