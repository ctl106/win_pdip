// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : pdip_p.h
// Description : Programmed Dialogue with Interactive Programs
//               Internal definitions
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



#ifndef PDIP_P_H
#define PDIP_P_H

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>



// ----------------------------------------------------------------------------
// Name   : pdip_ctx_t
// Usage  : User context
// ----------------------------------------------------------------------------
typedef struct pdip_ctx
{
  // Debug stream
  FILE *dbg_output;

  // Error stream
  FILE *err_output;

  // Controlled program and its parameters
  char **av;
  int ac;

  // CPU affinity of the controlled program
  size_t cpu_sz;
  unsigned char *cpu;

  // Flags
  int flags;

  // Master side of the PTY
  int pty_master;

  // Debug level
  int debug;

  // Process id of the controlled process
  volatile pid_t pid; // volatile as it is updated asynchronously by signal handler

  // Status of the dead controlled process
  volatile int status; // volatile as it is updated asynchronously by signal handler

  // State of the controlled process
  volatile sig_atomic_t state;  // volatile as it is updated asynchronously by signal handler
#define PDIP_STATE_INIT    0
#define PDIP_STATE_ALIVE   1
#define PDIP_STATE_ZOMBIE  2
#define PDIP_STATE_DEAD    3

  // Outstanding data
  char    *outstanding_data;
  size_t   outstanding_data_sz;
  size_t   outstanding_data_offset;

  size_t buf_resize_increment;

  struct pdip_ctx *next;
  struct pdip_ctx *prev;
} pdip_ctx_t;



// ----------------------------------------------------------------------------
// Name   : PDIP_ERR
// Usage  : Error messages
// ----------------------------------------------------------------------------
#define PDIP_ERR(ctx, format, ...) do {					\
    fprintf(((ctx) ? ((pdip_ctx_t *)(ctx))->err_output : stderr),	\
              "PDIP ERROR(%d) - (%s#%d): "format,                     \
              getpid(), __FUNCTION__, __LINE__, ## __VA_ARGS__);        \
                                 } while (0)


// ----------------------------------------------------------------------------
// Name   : pdip_debug_level
// Usage  : Global debug level
// ----------------------------------------------------------------------------
extern int pdip_debug_level;

// ----------------------------------------------------------------------------
// Name   : PDIP_DBG
// Usage  : Debug messages
// ----------------------------------------------------------------------------
#define PDIP_DBG(ctx, level, format, ...)			     \
  do { if ((ctx) && (((pdip_ctx_t *)(ctx))->debug >= (level)))	     \
                    fprintf(((pdip_ctx_t *)(ctx))->dbg_output,	     \
                            "PDIP(%d-%d) - %s#%d: "format,         \
                            getpid(), (level),			     \
                            __FUNCTION__, __LINE__, ## __VA_ARGS__); \
    else if (pdip_debug_level >= (level))                            \
                    fprintf(stderr,  	                             \
                            "PDIP(%d-%d) - %s#%d: "format,         \
                            getpid(), (level),			     \
                            __FUNCTION__, __LINE__, ## __VA_ARGS__); \
                } while(0)



#endif // PDIP_P_H
