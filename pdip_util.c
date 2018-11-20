// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : pdip_util.c
// Description : Utilities for Programmed Dialogue with Interactive Programs
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
//     13-Nov-2017  R. Koucha    - Creation
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#include <stdio.h>
#include <sys/types.h>


// ----------------------------------------------------------------------------
// Name   : ptr2char
// Usage  : Translate a pointer into a string of characters
// Return : None
// ----------------------------------------------------------------------------
static void ptr2char(
                        void          *ptr,
                        unsigned char *buf,
                        unsigned int  len
                       )
{
int           i;
unsigned long val = (unsigned long)ptr;
unsigned long val1;

  for (i = (int)(len - 1); i >= 0; i --)
  {
    val1 = val % 16;

    if (val1 < 10)
    {
      buf[i]  = (unsigned char)(val1 + '0');
    }
    else
    {
      buf[i]  = (unsigned char)((val1 - 10) + 'A');
    }

    if (val1 != val)
    {
      val /= 16;
    }
    else
    {
      val = 0;
    } /* end if */
  } /* end for */

} /* ptr2char */


static unsigned int blk_size = 512U;


// ----------------------------------------------------------------------------
// Name   : pdip_dump
// Usage  : Dump a memory zone on stderr
// Return : None
// ----------------------------------------------------------------------------
void pdip_dump(
               char   *buf,
               size_t  size_buf
              )
{
/* Line format : sizeof(addr)_bytes(16 * 2 + 15)_*ascii(16)* */
#define DUMP_LINE_SZ (sizeof(void *)*2 + 67)
char                line[DUMP_LINE_SZ + 1];
unsigned int        nb_line;
unsigned int        i;
unsigned int        last_line;
unsigned int        j;
unsigned int        k;
size_t              size_line;

  size_line = (blk_size >= 16 ? 16 : blk_size);

  nb_line = size_buf / size_line;

  last_line = size_buf % size_line;

  /* Initialization of the buffer */
  for (i = 0; i <= DUMP_LINE_SZ; i ++)
  {
    line[i] = ' ';
  }

  /* The stars */
  line[sizeof(void *)*2 + 49] = '*';
  line[sizeof(void *)*2 + 66] = '*';

  /* Terminating NUL */
  line[sizeof(void *)*2 + 67] = '\0';

  /* Index in the buffer */
  k = 0;

  for (i = 0; i < nb_line; i++)
  {
    /* Write the line address */
    ptr2char(&(buf[k]), (unsigned char *)line, sizeof(void *)*2);

    /* dump the "size_line" octets */
    for (j = 0; j < size_line; j ++, k++)
    {
      if (((unsigned char)(buf[k]) % 16) < 10)
      {
        (line + sizeof(void *)*2 + 1)[j * 3 + 1] = (char)(((unsigned char)(buf[k]) % 16) + '0');
      }
      else
      {
        (line + sizeof(void *)*2 + 1)[j * 3 + 1] = (char)((((unsigned char)(buf[k]) % 16) - 10) + 'A');
      }

      if (((unsigned char)(buf[k]) / 16) < 10)
      {
        (line + sizeof(void *)*2 + 1)[j * 3]     = (char)(((unsigned char)(buf[k]) / 16) + '0');
      }
      else
      {
        (line + sizeof(void *)*2 + 1)[j * 3]     = (char)((((unsigned char)(buf[k]) / 16) - 10) + 'A');
      }

      /* ASCII translation */
      if (((unsigned char)(buf[k]) >= 32) && ((unsigned char)(buf[k]) <= 127))
      {
        (line + sizeof(void *)*2 + 50)[j] = (char)(buf[k]);
      }
      else
      {
        (line + sizeof(void *)*2 + 50)[j] = '.';
      }
    } /* End for */

    fprintf(stderr, "%s\n", line);
  } /* End for */

  if (last_line)
  {
    /* Line number */
    ptr2char(&(buf[k]), (unsigned char *)line, sizeof(void *)*2);

    /* dump of 16 bytes */
    for (j = 0; j < last_line; j ++, k++)
    {
      if (((unsigned char)(buf[k]) % 16) < 10)
      {
        (line + sizeof(void *)*2 + 1)[j * 3 + 1] = (char)(((unsigned char)(buf[k]) % 16) + '0');
      }
      else
      {
        (line + sizeof(void *)*2 + 1)[j * 3 + 1] = (char)((((unsigned char)(buf[k]) % 16) - 10) + 'A');
      }

      if (((unsigned char)(buf[k]) / 16) < 10)
      {
        (line + sizeof(void *)*2 + 1)[j * 3]     = (char)(((unsigned char)(buf[k]) / 16) + '0');
      }
      else
      {
        (line + sizeof(void *)*2 + 1)[j * 3]     = (char)((((unsigned char)(buf[k]) / 16) - 10) + 'A');
      }

      /* ASCII translation */
      if ((unsigned char)(buf[k]) >= 32 && (unsigned char)(buf[k]) <= 127)
      {
        (line + sizeof(void *)*2 + 50)[j] = (char)(buf[k]);
      }
      else
      {
        (line + sizeof(void *)*2 + 50)[j] = '.';
      }
    } /* End for */

    for (j = last_line; j < size_line; j ++)
    {
      (line + sizeof(void *)*2 + 1 )[j * 3 + 1] = ' ';
      (line + sizeof(void *)*2 + 1 )[j * 3]     = ' ';
      (line + sizeof(void *)*2 + 50)[j]         = ' ';
    } /* End for */

    fprintf(stderr, "%s\n", line);
  } /* End if last_line */
} // pdip_dump
