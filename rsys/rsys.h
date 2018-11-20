// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : rsys.h
// Description : system() service based on shared background shells
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
// Evolutions  :
//
//     12-Jan-2018  R. Koucha    - Creation
//     26-Feb-2018  R. Koucha    - rsystem(): Added GCC format attribute
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#ifndef RSYS_H
#define RSYS_H

//----------------------------------------------------------------------------
// Name        : rsystem
// Description : Remote system() service (submit the command
//               interactively to a running background shell through
//               rsystemd server) 
// Return      : status of the executed command, if OK
//               -1, if error (errno is set)
//----------------------------------------------------------------------------
extern int rsystem(const char *fmt, ...) __attribute ((format (printf, 1, 2)));;


// ----------------------------------------------------------------------------
// Name   : rsys_lib_initialize
// Usage  : Library initialization when needed in a child process
// Return : 0, if OK
//          -1, if error (errno is set)
// ----------------------------------------------------------------------------
extern int rsys_lib_initialize(void);


#endif // RSYS_H
