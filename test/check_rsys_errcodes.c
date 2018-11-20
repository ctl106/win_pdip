// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : check_rsys_errcodes.c
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
//     26-Feb-2018  R. Koucha    - Creation
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "check_rsys.h"
#include "../rsys/rsys.h"
#include "../rsys/rsys_p.h"


// This is run once in the main process before each test cases processes
static void rsys_setup_unchecked_fixture(void)
{

  fprintf(stderr, "%s: %d\n", __FUNCTION__, getpid());


} // rsys_setup_unchecked_fixture


// This is run once inside each test case processes
static void rsys_setup_checked_fixture(void)
{
int   rc;
char *ptr;

  fprintf(stderr, "%s: %d\n", __FUNCTION__, getpid());

  // Define an impossible pathname for the server socket
  rc = setenv(RSYS_SOCKET_PATH_ENV, "/noexist/impossible/test/rsys_sock", 1);
  ck_assert_int_eq(rc, 0);

  rc = rsys_lib_initialize();
  ck_assert_int_eq(rc, -1);

  // Define a very long socket pathname to make snprintf() return an error
  // in the internal rsys_connect() function
#define CK_RSYS_PATH_LEN 8192
  ptr = (char *)malloc(CK_RSYS_PATH_LEN);
  ck_assert_ptr_ne(ptr, NULL);
  memset(ptr, 'A', CK_RSYS_PATH_LEN);
  ptr[CK_RSYS_PATH_LEN - 1] = '\0';
  rc = setenv(RSYS_SOCKET_PATH_ENV, ptr, 1);
  ck_assert_int_eq(rc, 0);
  free(ptr);

  rc = rsys_lib_initialize();
  ck_assert_int_eq(rc, -1);

  // Restore the environment variable
  rc = setenv(RSYS_SOCKET_PATH_ENV, ck_rsys_sock_var(0), 1);
  ck_assert_int_eq(rc, 0);

  // As the test cases are run in child processes, RSYS service
  // must be initialized inside them
  rc = rsys_lib_initialize();
  ck_assert_int_eq(rc, 0);
} // rsys_setup_checked_fixture




// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_rsystem_err)

int status;

  status = rsystem(0);
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

END_TEST




TCase *rsys_errcodes_tests(void)
{
TCase *tc_err_code;

  printf("%s: %d\n", __FUNCTION__, getpid());

  /* Error test case */
  tc_err_code = tcase_create("RSYS error codes");

  // Increase the timeout for the test case as we use lots of timeouts
  tcase_set_timeout(tc_err_code, 60);

  tcase_add_unchecked_fixture(tc_err_code, rsys_setup_unchecked_fixture, 0);
  tcase_add_checked_fixture(tc_err_code, rsys_setup_checked_fixture, 0);
  tcase_add_test(tc_err_code, test_rsystem_err);

  return tc_err_code;
} // rsys_errcodes_tests
