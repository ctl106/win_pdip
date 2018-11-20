// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : check_isys_api.c
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
#include <sys/wait.h>
#include <malloc.h>
#include <sys/wait.h>

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
int rc;

  printf("%s: %d\n", __FUNCTION__, getpid());

  rc = setenv(ISYS_ENV_TIMEOUT, "5", 1);
  ck_assert_int_eq(rc, 0);

  // As the test cases are run in child processes, ISYS service
  // must be initialize inside them
  (void)isys_lib_initialize();

} // isys_setup_checked_fixture




// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_env)

int status;

  status = isystem("%s", "/bin/date");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

END_TEST


TCase *isys_api_env_tests(void)
{
TCase *tc_api_env;

  printf("%s: %d\n", __FUNCTION__, getpid());

  /* Error test case */
  tc_api_env = tcase_create("ISYS API WITH ENV VAR");

  // Increase the timeout for the test case as we use lots of timeouts
  tcase_set_timeout(tc_api_env, 60);

  tcase_add_unchecked_fixture(tc_api_env, isys_setup_unchecked_fixture, 0);
  tcase_add_checked_fixture(tc_api_env, isys_setup_checked_fixture, 0);
  tcase_add_test(tc_api_env, test_env);

  return tc_api_env;
} // isys_api_env_tests
