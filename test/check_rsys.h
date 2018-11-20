// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : check_rsys.h
// Description : Unitary test for RSYS
//               
// License     :
//
//  Copyright (C) 2007-2018 Rachid Koucha <rachid dot koucha at gmail dot com>
//
//
//  This program is free and destined to help people using RSYS library
//
//
// Evolutions  :
//
//     07-Mar-2018  R. Koucha    - Creation
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=





#ifndef CHECK_RSYS_H
#define CHECK_RSYS_H

#include <check.h>


extern void rsys_startup(void);

extern void rsys_end(void);

extern int rsys_wait_child(pid_t pid);


// ----------------------------------------------------------------------------
// Name   : ck_rsys_sock_var
// Usage  : Make the pathname of a server socket
// Return : Pathname of the server socket (static string, not reentrant)
// ----------------------------------------------------------------------------
extern char *ck_rsys_sock_var(unsigned int srv_nb);



// ----------------------------------------------------------------------------
// Name   : ck_rsys_child_rsystem
// Usage  : Call rsystem() in a child process
// Return : In the father : pid of the child, if OK or -1 if error
//          The child : its exit status is the result of the rsystem() call
// ----------------------------------------------------------------------------
extern int ck_rsys_child_rsystem(
                          const char *cmd,
                          unsigned int srv_nb
			  );





// ----------------------------------------------------------------------------
// Name   : rsys_errcodes_tests
// Usage  : Test units to check error codes
// Return : Test case
// ----------------------------------------------------------------------------
extern TCase *rsys_errcodes_tests(void);

// ----------------------------------------------------------------------------
// Name   : rsys_api_tests
// Usage  : Test units for API
// Return : Test case
// ----------------------------------------------------------------------------
extern TCase *rsys_api_tests(void);

// ----------------------------------------------------------------------------
// Name   : rsys_rsystemd_tests
// Usage  : Test units for RSYSTEMD
// Return : Test case
// ----------------------------------------------------------------------------
extern TCase *rsys_rsystemd_tests(void);



#endif // CHECK_RSYS_H
