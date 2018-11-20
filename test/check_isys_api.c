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
//     26-Feb-2018 R. Koucha    - Creation
//     22-Mar-2018 R. Koucha    - Added check of man examples
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <malloc.h>
#include <sys/wait.h>
#include <errno.h>

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
START_TEST(test_isystem)

int status;
int rc;

  // Empty command
  status = isystem("%s", "");
  ck_assert_int_eq(status, 0);

  // Command line which triggers a dynamic allocation
  status = isystem("%s", "echo this is a very long command followed by a very long comment in order to make ISYS allocate dynamic memory when formatting the command line # this is the comment following the very long command line to make this command line very very long (at least more than 256 characters");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

  status = isystem("/qwerty/azerty/cmd_not_exist");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 127);

  status = isystem("%s", "/bin/date");
  ck_assert_int_eq(status, 0);

  status = isystem("%s", "/bin/date --bad_param");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  status = isystem("%s", "test/pterm -e 4");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 4);

  status = isystem("%s", "test/pterm -k 15");
  ck_assert(WIFSIGNALED(status));
  ck_assert_int_eq(WTERMSIG(status), 15);

  status = isystem("%s", "test/pterm -k 9");
  ck_assert(WIFSIGNALED(status));
  ck_assert_int_eq(WTERMSIG(status), 9);

  // Program which does not display anything
  status = isystem("%s", "/bin/sleep 1");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

  // Fork a child into which PDIP/ISYS are deactivated by default
  rc = check_fork();
  ck_assert_int_ne(rc, -1);
  if (rc)
  {
    // Father
    do
    {
      rc = waitpid(rc, &status, 0);
    } while ((-1 == rc) && (EINTR == errno));
    ck_assert_int_ne(rc, -1);
    ck_assert(WIFEXITED(status));
    ck_assert_int_eq(WEXITSTATUS(status), 0);
  }
  else // Child
  {
    status = isystem("%s", "/bin/sleep 1");
    ck_assert_int_eq(status, -1);
    exit(0);
  }

  // Multiple commands
  status = isystem("%s; %s; %s", "/bin/echo tyty", "/bin/sleep 1", "/bin/date");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

  // Very verbose program
  status = isystem("%s", "test/pdata -B \"VERBOSE PROGRAM\" -b 1055 -l 117 -T \"END OF VERBOSE PROGRAM\"");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

  // Command ended with line feeds and spaces
  status = isystem("%s", "/bin/date;/bin/sleep 1 \n \n\n \f \r \v \t");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

  // We end the test case here as we are desynchronized with the shell

END_TEST




// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_man)

int status;

  status = system("test/tisys echo example for man");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

END_TEST




TCase *isys_api_tests(void)
{
TCase *tc_api;

  printf("%s: %d\n", __FUNCTION__, getpid());

  /* Error test case */
  tc_api = tcase_create("ISYS API");

  // Increase the timeout for the test case as we use lots of timeouts
  tcase_set_timeout(tc_api, 60);

  tcase_add_unchecked_fixture(tc_api, isys_setup_unchecked_fixture, 0);
  tcase_add_checked_fixture(tc_api, isys_setup_checked_fixture, 0);
  tcase_add_test(tc_api, test_isystem);
  tcase_add_test(tc_api, test_man);

  return tc_api;
} // isys_api_tests
