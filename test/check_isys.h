// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : check_isys.h
// Description : Unitary test for ISYS
//               
// License     :
//
//  Copyright (C) 2007-2018 Rachid Koucha <rachid dot koucha at gmail dot com>
//
//
//  This program is free and destined to help people using ISYS library
//
//
// Evolutions  :
//
//     26-Feb-2018  R. Koucha    - Creation
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=





#ifndef CHECK_ISYS_H
#define CHECK_ISYS_H

#include <check.h>



// ----------------------------------------------------------------------------
// Name   : isys_errcodes_tests
// Usage  : Test units to check error codes
// Return : Test case
// ----------------------------------------------------------------------------
extern TCase *isys_errcodes_tests(void);

// ----------------------------------------------------------------------------
// Name   : isys_api_tests
// Usage  : Test units for API
// Return : Test case
// ----------------------------------------------------------------------------
extern TCase *isys_api_tests(void);


// ----------------------------------------------------------------------------
// Name   : isys_api_env_tests
// Usage  : Test units for API with environment variable
// Return : Test case
// ----------------------------------------------------------------------------
extern TCase *isys_api_env_tests(void);



#endif // CHECK_ISYS_H
