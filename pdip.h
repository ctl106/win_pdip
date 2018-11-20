// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : pdip.h
// Description : Programmed Dialogue with Interactive Programs
// License     :
//
//  Copyright (C) 2007-2018 Rachid Koucha <rachid dot koucha at gmail dot com>
//
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
// Evolutions  :
//
//     06-Nov-2017  R. Koucha    - Creation
//     04-Dec-2017  R. Koucha    - Changed prototype of pdip_status()
//     15-Jan-2018  R. Koucha    - Added pdip_fd()
//     22-Jan-2018  R. Koucha    - Added pdip_cpu_zero(), pdip_cpu_all()
//     25-Jan-2018  R. Koucha    - Added PDIP_FLAG_RECV_ON_THE_FLOW
//     26-Feb-2018  R. Koucha    - pdip_send(): Added GCC format attribute
//     01-Mar-2018  R. Koucha    - Added pdip_lib_initialize() for child
//                                 processes
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#ifndef PDIP_H
#define PDIP_H

#include <sys/types.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>




// ----------------------------------------------------------------------------
// Name   : pdip_t
// Usage  : PDIP context
// ----------------------------------------------------------------------------
typedef void *pdip_t;



// ----------------------------------------------------------------------------
// Name   : pdip_configure
// Usage  : Configuration of PDIP
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_configure(
                    int sig_hdl_internal,
                    int debug_level
		    );


// ----------------------------------------------------------------------------
// Name   : pdip_signal_handler
// Usage  : Signal handler
// Return : PDIP_SIG_HANDLED, if the signal has been handled by the service
//          PDIP_SIG_UNKNOWN, if the signal has not been handled by the service
//          PDIP_SIG_ERROR, if error
// ----------------------------------------------------------------------------
extern int pdip_signal_handler(
                        int        sig,   // Received signal
                        siginfo_t *info   // Context of the signal
			);

#define PDIP_SIG_HANDLED  0
#define PDIP_SIG_UNKNOWN  1
#define PDIP_SIG_ERROR    2




// ----------------------------------------------------------------------------
// Name   : pdip_cfg_t
// Usage  : Configuration of a PDIP object
// ----------------------------------------------------------------------------
typedef struct
{
  FILE *dbg_output;   // Stream into which are displayed the debug messages
                      // of the PDIP object
                      // If NULL, it defaults to stderr
  FILE *err_output;   // Stream into which are displayed the error messages
                      // of the PDIP object
                      // If NULL, it defaults to stderr

  int debug_level;    // Debug level of the PDIP object. The higher the value,
                      // the more debug messages are displayed
                      // Default: 0 (no debug messages)

  unsigned int flags;

#define PDIP_FLAG_ERR_REDIRECT     0x01  // If set, the stderr of the controlled
                                         // process is also redirected to the
                                         // main program.
                                         // Otherwise, it is inherited from the
                                         // main program (default)

#define PDIP_FLAG_RECV_ON_THE_FLOW 0x02  // If set, data are returned to the user as
                                         // they are received even if the regular expression
                                         // is not found (returned code is PDIP_RECV_DATA)
                                         // Otherwise, the data is returned when
                                         // the regular expression is found (default)

  unsigned char *cpu;  // Array of bits describing the CPU affinity of the controlled process
                       // Allocated/freed with pdip_cpu_alloc()/pdip_cpu_free()
                       // By default, the affinity is inherited from the main program

  size_t buf_resize_increment;   // Amount of space in bytes to add to the reception
                                 // buffer each time additional space is needed
                                 // Default is 1 KB

} pdip_cfg_t;



// ----------------------------------------------------------------------------
// Name   : pdip_cfg_init
// Usage  : Initialize the configuration of a PDIP object to its default values
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_cfg_init(
                  pdip_cfg_t *cfg
		  );


// ----------------------------------------------------------------------------
// Name   : pdip_cpu_nb
// Usage  : Return the number of CPUs
// Return : Number of CPUs
// ----------------------------------------------------------------------------
extern unsigned int pdip_cpu_nb(void);


// ----------------------------------------------------------------------------
// Name   : pdip_cpu_alloc
// Usage  : Allocate a bitmap of CPUs
// Return : bitmap, if OK
//          0, if error (errno is set)
// ----------------------------------------------------------------------------
extern unsigned char *pdip_cpu_alloc(void);


// ----------------------------------------------------------------------------
// Name   : pdip_cpu_free
// Usage  : Free a bitmap of CPUs
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_cpu_free(unsigned char *cpu);


// ----------------------------------------------------------------------------
// Name   : pdip_cpu_zero
// Usage  : Reset a bitmap of CPUs
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_cpu_zero(unsigned char *cpu);


// ----------------------------------------------------------------------------
// Name   : pdip_cpu_all
// Usage  : Set all the bits in a bitmap of CPUs
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_cpu_all(unsigned char *cpu);


// ----------------------------------------------------------------------------
// Name   : pdip_cpu_set
// Usage  : Set the CPU number n in the bitmap
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_cpu_set(
                  unsigned char *cpu,
                  unsigned int   n
		  );


// ----------------------------------------------------------------------------
// Name   : pdip_cpu_isset
// Usage  : Check if the CPU number n is set in the bitmap
// Return : 1, if CPU number is set
//          0, if CPU number is not set
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_cpu_isset(
                   unsigned char *cpu,
                   unsigned int   n
		   );


// ----------------------------------------------------------------------------
// Name   : pdip_cpu_unset
// Usage  : Unset the CPU number n in the bitmap
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_cpu_unset(
                   unsigned char *cpu,
                   unsigned int   n
		   );


// ----------------------------------------------------------------------------
// Name   : pdip_new
// Usage  : Allocate a PDIP context
// Return : PDIP object, if OK
//          0, if error (errno is set)
// ----------------------------------------------------------------------------
extern pdip_t pdip_new(
                        pdip_cfg_t *cfg
                      );


// ----------------------------------------------------------------------------
// Name   : pdip_delete
// Usage  : Deallocate a PDIP context
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_delete(
                pdip_t  ctx,
                int    *status
		);


// ----------------------------------------------------------------------------
// Name   : pdip_fd
// Usage  : Return the file descriptor of a PDIP object
// Return : File descriptor, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_fd(pdip_t ctx);


// ----------------------------------------------------------------------------
// Name   : pdip_status
// Usage  : Return the status of the dead controlled program
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_status(
                pdip_t  ctx,
                int    *status,
                int     blocking
		);

// ----------------------------------------------------------------------------
// Name   : pdip_set_debug_level
// Usage  : Set the debug level for a given PDIP context (ctx != 0) or
//          set the global debug level
// Return : 0, if OK
//          !0, if error (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_set_debug_level(
                         pdip_t ctx,
                         int    level
			 );



// ----------------------------------------------------------------------------
// Name   : pdip_exec
// Usage  : Execute the process to be controlled
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_exec(
                     pdip_t  ctx,
                     int     ac,
                     char   *av[]
	            );


// ----------------------------------------------------------------------------
// Name   : pdip_recv
// Usage  : Receive data from the controlled process
// Return : PDIP_RECV_FOUND
//          PDIP_RECV_TIMEOUT
//          PDIP_RECV_DATA
//          PDIP_RECV_ERROR (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_recv(
              pdip_t          *ctx,
	      const char      *regular_expr,
              char           **display,
              size_t          *display_sz,
              size_t          *data_sz,
              struct timeval  *timeout
	      );

#define PDIP_RECV_ERROR    -1 // Reception error, errno is set, data may have
                              // been received (data_sz >= 0)
#define PDIP_RECV_FOUND     0 // Regular expression found (data_sz > 0)
#define PDIP_RECV_TIMEOUT   1 // Timeout, regular expression not found or
                              // no data arrived (data_sz >= 0)
#define PDIP_RECV_DATA      2 // . No regular expression passed, infinite
                              // timeout, data arrived (data_sz > 0)
                              // . Regular expression still not found but
                              // data arrived and PDIP_FLAG_RECV_ON_THE_FLOW
                              // is set


// ----------------------------------------------------------------------------
// Name   : pdip_send
// Usage  : Send a formated string to the controlled process
// Return : Amount of sent data, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_send(
              pdip_t      ctx,
              const char *format,
              ...
	      ) __attribute ((format (printf, 2, 3)));


// ----------------------------------------------------------------------------
// Name   : pdip_flush
// Usage  : Flush the outstanding data
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_flush(
               pdip_t    ctx,
               char    **display,
               size_t   *display_sz,
               size_t   *data_sz
	       );


// ----------------------------------------------------------------------------
// Name   : pdip_sig
// Usage  : Send a signal to the controlled process
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_sig(
              pdip_t      ctx,
              int         sig
	      );



// ----------------------------------------------------------------------------
// Name   : pdip_lib_initialize
// Usage  : Library initialization when needed in a child process
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int pdip_lib_initialize(void);



#endif // PDIP_H
