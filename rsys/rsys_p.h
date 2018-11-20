// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : rsys_p.h
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
//     14-Jan-2018  R. Koucha    - Creation
//     02-MAr-2018  R. Koucha    - Added environment variable for socket
//                                 pathname
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#ifndef RSYS_P_H
#define RSYS_P_H


#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>




// ----------------------------------------------------------------------------
// Name   : RSYS_ERR
// Usage  : Error messages
// ----------------------------------------------------------------------------
#define RSYS_ERR(format, ...) do {                               \
    fprintf(stderr,                                              \
            "RSYS ERROR(%d) - (%s#%d): "format,                  \
            getpid(), __FUNCTION__, __LINE__, ## __VA_ARGS__);   \
                                 } while (0)


// ----------------------------------------------------------------------------
// Name   : RSYSD_ERR
// Usage  : Error messages
// ----------------------------------------------------------------------------
#define RSYSD_ERR(format, ...) do {                              \
    fprintf(stderr,                                              \
            "RSYSD ERROR(%d) - (%s#%d): "format,                 \
            getpid(), __FUNCTION__, __LINE__, ## __VA_ARGS__);   \
                                 } while (0)


// ----------------------------------------------------------------------------
// Name   : RSYSD_DBG
// Usage  : Debug messages
// ----------------------------------------------------------------------------
#define RSYSD_DBG(level, format, ...) do {			 \
    if (level <= rsysd_dbg_level)                                \
      fprintf(stderr,                                            \
              "RSYSD DBG(%d, %d) - (%s#%d): "format,             \
              getpid(), level, __FUNCTION__, __LINE__, ## __VA_ARGS__);	\
                                 } while (0)


// ----------------------------------------------------------------------------
// Name   : RSYS_SOCKET_PATH
// Usage  : Pathname of the socket (default)
// ----------------------------------------------------------------------------
#define RSYS_SOCKET_PATH  "/var/run/rsys.socket"


// ----------------------------------------------------------------------------
// Name   : RSYS_SOCKET_PATH_ENV
// Usage  : Alternate pathname of the socket in environment
// ----------------------------------------------------------------------------
#define RSYS_SOCKET_PATH_ENV  "RSYS_SOCKET_PATH"



// ----------------------------------------------------------------------------
// Name   : rsys_msg_t
// Usage  : Message between librsys and rsystemd
// ----------------------------------------------------------------------------
typedef struct
{
  int type;
#define RSYS_MSG_CMD      0
#define RSYS_MSG_DISPLAY  1
#define RSYS_MSG_EOC      2
#define RSYS_MSG_BUSY     3
#define RSYS_MSG_OOM      4

  size_t length;     // Length of the following data

  union
  {
    int status;    // For EOC
  } data;

} rsys_msg_t;



//---------------------------------------------------------------------------
// Name : rsysd_send_msg
// Usage: Send a message without data
// Return: 0, if OK
//         -1, if error
//----------------------------------------------------------------------------
extern int rsys_send_msg(
                         int sd,
                         int type
		        );


//---------------------------------------------------------------------------
// Name : rsysd_send_eoc
// Usage: Send a EOC message
// Return: 0, if OK
//         -1, if error
//----------------------------------------------------------------------------
extern int rsys_send_eoc(
                         int sd,
                         int status
		        );


//---------------------------------------------------------------------------
// Name : rsysd_send_msg_data
// Usage: Send a message with data
// Return: 0, if OK
//         -1, if error
//----------------------------------------------------------------------------
extern int rsys_send_msg_data(
                  int     sd,
                  int     type,
                  size_t  length,
                  char   *data
		  );


#endif // RSYS_P_H
