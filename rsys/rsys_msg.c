// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : rsys_msg.c
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
//     17-Jan-2018  R. Koucha    - Creation
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include "rsys_p.h"


//---------------------------------------------------------------------------
// Name : rsysd_send_msg
// Usage: Send a message without data
// Return: 0, if OK
//         -1, if error
//----------------------------------------------------------------------------
int rsys_send_msg(
                  int sd,
                  int type
		 )
{
rsys_msg_t msg;
int        rc;

  msg.type = type;
  msg.length = 0;
  rc = write(sd, &msg, sizeof(msg));
  if (rc != sizeof(msg))
  {
    if (rc >= 0)
    {
      errno = EIO;
    }
    return -1;
  }

  return 0;
} // rsys_send_msg


//---------------------------------------------------------------------------
// Name : rsysd_send_eoc
// Usage: Send a EOC message
// Return: 0, if OK
//         -1, if error
//----------------------------------------------------------------------------
int rsys_send_eoc(
                  int sd,
                  int status
		 )
{
rsys_msg_t msg;
int        rc;

  msg.type = RSYS_MSG_EOC;
  msg.length = 0;
  msg.data.status = status; 
  rc = write(sd, &msg, sizeof(msg));
  if (rc != sizeof(msg))
  {
    if (rc >= 0)
    {
      errno = EIO;
    }
    return -1;
  }

  return 0;
} // rsys_send_eoc


//---------------------------------------------------------------------------
// Name : rsysd_send_msg_data
// Usage: Send a message with data
// Return: 0, if OK
//         -1, if error
//----------------------------------------------------------------------------
int rsys_send_msg_data(
                  int     sd,
                  int     type,
                  size_t  length,
                  char   *data
		 )
{
rsys_msg_t msg;
int        rc;

  // Send the header
  msg.type = type;
  msg.length = length;
  rc = write(sd, &msg, sizeof(msg));
  if (rc != sizeof(msg))
  {
    if (rc >= 0)
    {
      errno = EIO;
    }
    return -1;
  }

  // Send the data
  rc = write(sd, data, length);
  if (rc != (int)length)
  {
    if (rc >= 0)
    {
      errno = EIO;
    }
    return -1;
  }

  return 0;
} // rsys_send_msg_data
