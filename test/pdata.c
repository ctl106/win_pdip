// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : pdata.c
// Description : Print data on standard output
//               
// License     :
//
//  Copyright (C) 2007-2018 Rachid Koucha <rachid dot koucha at gmail dot com>
//
//  This program is free and destined to help people using PDIP/ISYS/RSYS
//  libraries
//
// Evolutions  :
//
//     24-Nov-2017  R. Koucha    - Creation
//     26-Dec-2018  R. Koucha    - Added call to pdip_cfg_init()
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>




int main(int ac, char *av[])
{
int           forever;
int           nb;
int           lines;
unsigned int  i, j;
int           opt;
char         *interactive;
char          str[2048];
char         *banner;
char         *trailer;

  forever = 0;
  nb = 0;
  lines = 1;
  interactive = 0;
  banner = 0;
  trailer = 0;

  while ((opt = getopt(ac, av, "T:B:i:b:k:l:f")) != EOF)
  {
    switch(opt)
    {
      case 'B' : // Banner to display
      {
        banner = optarg;
      }
      break;

      case 'i': // Prompt for the interactive mode
      {
        interactive = optarg;
      }
      break;

      case 'b': // Number of bytes per line
      {
        nb = atoi(optarg);
      }
      break;

      case 'k': // Number of kilobytes per line
      {
        nb = atoi(optarg) * 1024;
      }
      break;

      case 'f' :
      {
        forever = 1;
      }
      break;

      case 'l': // Number of lines
      {
        lines = atoi(optarg);
      }
      break;

      case 'T' : // Trailer to display
      {
        trailer = optarg;
      }
      break;

      default:
      {
        return 1;
      }
    } // End switch
  } // End while

  if (banner)
  {
    printf("%s\n", banner);
  }

  if (interactive)
  {
    // Print the prompts on stderr as it is not buffered (no need ending '\n' to be printed)
    fprintf(stderr, "%s", interactive);
    while (fgets(str, sizeof(str), stdin))
    {
      fprintf(stderr, "%s", interactive);
    } // End while

    return 0;
  } // End if interactive mode

  for (j = 0; j < (unsigned)lines; j ++)
  {
    for (i = 0; i < (unsigned)nb; i ++)
    {
      printf("%c", 'a' + (i % 26));
    } // End for

    printf("\n");

  } // End for

  if (trailer)
  {
    printf("%s\n", trailer);
  }

  if (forever)
  {
    pause();
  }

  return 0;

} // main
