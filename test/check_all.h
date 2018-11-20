// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : check_all.h
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
//     28-Mar-2018  R. Koucha    - Creation
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=





#ifndef CHECK_ALL_H
#define CHECK_ALL_H

#include <stdio.h>
#include <errno.h>
#include <libgen.h>
#include <check.h>


// ----------------------------------------------------------------------------
// Name   : ck_assert_errno_eq
// Usage  : checks the value of errno
// Return : None
// ----------------------------------------------------------------------------
#define ck_assert_errno_eq(e) ck_assert_int_eq(errno, (e))



// ----------------------------------------------------------------------------
// Name   : ck_pdip_assert
// Usage  : Assert which prints a formatted message
// ----------------------------------------------------------------------------
#define ck_pdip_assert(e, format, ...) do { if (!(e)) {    \
           fprintf(stderr, \
                  "!!! PDIP_ASSERT_%d(%s/%s#%d) '%s' !!! ==> " format, \
                   getpid(), basename(__FILE__), __FUNCTION__, __LINE__, #e, ## __VA_ARGS__); \
       abort();           \
    } } while(0);



#endif // CHECK_ALL_H
