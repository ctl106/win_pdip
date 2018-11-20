// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : isys.h
// Description : system() service based on PDIP and a remanent background
//               shell to avoid fork/exec() of the current process
//
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
//     11-Nov-2017  R. Koucha    - Creation
//     26-Feb-2018  R. Koucha    - isystem(): Added GCC format attribute
//     01-Mar-2018  R. Koucha    - Added isys_lib_initialize() for child
//                                 processes
//                               - Added ISYS_ENV_TIMEOUT environment variable
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#ifndef ISYS_H
#define ISYS_H


//----------------------------------------------------------------------------
// Name        : ISYS_ENV_TIMEOUT
// Description : Environment variable specifying the timeout in seconds to
//               receive the data from the shell
//----------------------------------------------------------------------------
#define ISYS_ENV_TIMEOUT "ISYS_TIMEOUT"


//----------------------------------------------------------------------------
// Name        : isystem
// Description : Interactive system() service (submit the command
//               interactively to a running background shell) 
// Return      : status of the executed command, if OK
//               -1, if error (errno is set)
//----------------------------------------------------------------------------
extern int isystem(const char *fmt, ...) __attribute ((format (printf, 1, 2)));



// ----------------------------------------------------------------------------
// Name   : isys_lib_initialize
// Usage  : Library initialization when needed in a child process
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int isys_lib_initialize(void);


#endif // ISYS_H
