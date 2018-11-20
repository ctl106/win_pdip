// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : pdip_util.h
// Description : Utilities for Programmed Dialogue with Interactive Programs
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
//     13-Nov-2017  R. Koucha    - Creation
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=



#ifndef PDIP_UTIL_H
#define PDIP_UTIL_H


#include <sys/types.h>
#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>




// ----------------------------------------------------------------------------
// Name   : pdip_assert
// Usage  : Assert which prints a formatted message
// ----------------------------------------------------------------------------
#define pdip_assert(e, format, ...) do { if (!(e)) {	\
           fprintf(stderr, \
                  "!!! PDIP_ASSERT_%d(%s/%s#%d) '%s' !!! ==> " format, \
		   getpid(), basename(__FILE__), __FUNCTION__, __LINE__, #e, ## __VA_ARGS__); \
       abort();								\
    } } while(0);



// ----------------------------------------------------------------------------
// Name   : pdip_dump
// Usage  : Dump a memory zone on stderr
// Return : None
// ----------------------------------------------------------------------------
extern void pdip_dump(
                      char   *buf,
                      size_t  size_buf
	             );


// ----------------------------------------------------------------------------
// Name   : PDIP_DUMP
// Usage  : Print a formatted string followed by a dump of a buffer (b) of 'l'
//          bytes
// ----------------------------------------------------------------------------
#define PDIP_DUMP(ctx, level, format, b, l, ...)		    \
            do { if ((ctx)->debug >= (level))			    \
                 {                                                  \
                   fprintf(stderr, "\nPDIP(%d-%d) - %s#%d: "format, \
			   getpid(), (level),			    \
                           __FUNCTION__, __LINE__, ## __VA_ARGS__); \
                   pdip_dump((b), (l));                             \
                            }                                       \
                          } while(0)



#endif // PDIP_UTIL_H
