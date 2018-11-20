// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : check_isys_errcodes.c
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


#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#include "check_isys.h"
#include "../isys/isys.h"


// This is run once in the main process before each test cases
static void isys_setup_unchecked_fixture(void)
{

  printf("%s: %d\n", __FUNCTION__, getpid());

} // isys_setup_unchecked_fixture


// This is run once inside each test case processes
static void isys_setup_checked_fixture(void)
{
  printf("%s: %d\n", __FUNCTION__, getpid());

  // As the test cases are run in child processes, ISYS service
  // must be initialize inside them
  (void)isys_lib_initialize();

} // isys_setup_checked_fixture




// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_isystem_err)

int         status;

  status = isystem(0);
  ck_assert_int_eq(status, 0);

END_TEST




TCase *isys_errcodes_tests(void)
{
TCase *tc_err_code;

  printf("%s: %d\n", __FUNCTION__, getpid());

  /* Error test case */
  tc_err_code = tcase_create("ISYS error codes");

  // Increase the timeout for the test case as we use lots of timeouts
  tcase_set_timeout(tc_err_code, 60);

  tcase_add_unchecked_fixture(tc_err_code, isys_setup_unchecked_fixture, 0);
  tcase_add_checked_fixture(tc_err_code, isys_setup_checked_fixture, 0);
  tcase_add_test(tc_err_code, test_isystem_err);

  return tc_err_code;
} // isys_errcodes_tests
