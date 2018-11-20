// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : check_pdip.h
// Description : Unitary test for Programmed Dialogue with Interactive Programs
//               
// License     :
//
//  Copyright (C) 2007-2018 Rachid Koucha <rachid dot koucha at gmail dot com>
//
//
//  This program is free and destined to help people using PDIP library
//
//
// Evolutions  :
//
//     29-Jan-2018  R. Koucha    - Creation
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=





#ifndef CHECK_PDIP_H
#define CHECK_PDIP_H


#include <check.h>



// ----------------------------------------------------------------------------
// Name   : CK_PDIP_PROMPT
// Usage  : Prompt used in the shells
// Return : None
// ----------------------------------------------------------------------------
#define CK_PDIP_PROMPT "PRompt> "





// ----------------------------------------------------------------------------
// Name   : pdip_errcodes_tests
// Usage  : Test units to check error codes
// Return : Test case
// ----------------------------------------------------------------------------
extern TCase *pdip_errcodes_tests(void);

// ----------------------------------------------------------------------------
// Name   : pdip_api_tests
// Usage  : Test units for API
// Return : Test case
// ----------------------------------------------------------------------------
extern TCase *pdip_api_tests(void);


#endif // CHECK_PDIP_H
