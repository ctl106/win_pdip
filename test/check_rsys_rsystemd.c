// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : check_rsys_rsystemd.c
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
//     15-Mar-2018  R. Koucha    - Creation
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <malloc.h>
#include <sys/wait.h>

#include "check_rsys.h"
#include "../rsys/rsys.h"
#include "../rsys/rsys_p.h"












// This is run once in the main process before each test cases
static void rsys_setup_unchecked_fixture(void)
{

  fprintf(stderr, "%s: %d\n", __FUNCTION__, getpid());

} // rsys_setup_unchecked_fixture


// This is run once inside each test case processes
static void rsys_setup_checked_fixture(void)
{
int rc;

  fprintf(stderr, "%s: %d\n", __FUNCTION__, getpid());

  // Set the environment variable for the server socket
  rc = setenv(RSYS_SOCKET_PATH_ENV, ck_rsys_sock_var(0), 1);
  ck_assert_int_eq(rc, 0);

  // As the test cases are run in child processes, RSYS service
  // must be initialized inside them
  rc = rsys_lib_initialize();
  ck_assert_int_eq(rc, 0);

} // rsys_setup_checked_fixture



// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_rsystemd)

int   status;
char *ptr;
char  sav_env[256];
int   rc;

  // Display help
  status = rsystem("rsys/rsystemd -h");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

  // Display version
  status = rsystem("rsys/rsystemd -V");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

  // Display version with "-s" option to increase coverage test
  status = rsystem("rsys/rsystemd -V -s 0");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

  // Bad option
  status = rsystem("rsys/rsystemd -u");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  // Define a very long socket pathname to make snprintf() return an error
  // in the internal rsysd_open_socket() function
  rc = snprintf(sav_env, sizeof(sav_env), "%s", getenv(RSYS_SOCKET_PATH_ENV));
  ck_assert_int_lt(rc, sizeof(sav_env));

#define CK_RSYS_PATH_LEN 8192
  ptr = (char *)malloc(CK_RSYS_PATH_LEN);
  ck_assert_ptr_ne(ptr, NULL);
  memset(ptr, 'A', CK_RSYS_PATH_LEN);
  ptr[CK_RSYS_PATH_LEN - 1] = '\0';
  rc = setenv(RSYS_SOCKET_PATH_ENV, ptr, 1);
  ck_assert_int_eq(rc, 0);
  free(ptr);

  // We call system() and not rsystem() to take in account the environment
  // variable
  status = system("rsys/rsystemd");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  // Restore the environment variable
  rc = setenv(RSYS_SOCKET_PATH_ENV, sav_env, 1);
  ck_assert_int_eq(rc, 0);


  // Define an impossible pathname to make bind() fail in the
  // internal rsysd_open_socket() function
  rc = snprintf(sav_env, sizeof(sav_env), "%s", getenv(RSYS_SOCKET_PATH_ENV));
  ck_assert_int_lt(rc, sizeof(sav_env));

  rc = setenv(RSYS_SOCKET_PATH_ENV, "/impossible/foo/bar/not_exist/s", 1);
  ck_assert_int_eq(rc, 0);

  // We call system() and not rsystem() to take in account the environment
  // variable
  status = system("rsys/rsystemd");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 1);


  // Various syntax errors in the shell parameter
  rc = setenv(RSYS_SOCKET_PATH_ENV, ck_rsys_sock_var(100), 1);
  ck_assert_int_eq(rc, 0);

  status = system("rsys/rsystemd -s");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  status = system("rsys/rsystemd -s _");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  status = system("rsys/rsystemd -s :_");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  status = system("rsys/rsystemd -s 4,t");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  status = system("rsys/rsystemd -s 4,8t");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  status = system("rsys/rsystemd -s 4,80t");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  status = system("rsys/rsystemd -s 80t");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  status = system("rsys/rsystemd -s 0-_");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  status = system("rsys/rsystemd -s 0-1_");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  status = system("rsys/rsystemd -s 0-11_");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  status = system("rsys/rsystemd -s 1-0");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  status = system("rsys/rsystemd -s 10-0");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  // Restore the environment variable
  rc = setenv(RSYS_SOCKET_PATH_ENV, sav_env, 1);
  ck_assert_int_eq(rc, 0);


  //
  // Daemon mode but with a syntax error in the shell
  // to make sure that the daemon will fail
  //
  rc = setenv(RSYS_SOCKET_PATH_ENV, ck_rsys_sock_var(100), 1);
  ck_assert_int_eq(rc, 0);

  // The daemon fails but as it is a child process. The father exits with code
  // 0 anyway
  status = system("rsys/rsystemd -D -s 0:1''");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

  // Restore the environment variable
  rc = setenv(RSYS_SOCKET_PATH_ENV, sav_env, 1);
  ck_assert_int_eq(rc, 0);

END_TEST



// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_example)

int   status;

  // Check trsys example presented in "man rsystem"
  status = system("test/trsys echo example");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

END_TEST


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_debug)

int  status;
int  rc;

  // The server#5 is running in debug mode
  rc = setenv(RSYS_SOCKET_PATH_ENV, ck_rsys_sock_var(5), 1);
  ck_assert_int_eq(rc, 0);

  // To take in account the socket name specified by the preceding environment
  // variable, we launch system(trsys) which triggers a new process which
  // connects on this socket.

  // Check trsys example presented in "man rsystem"
  status = system("test/trsys /bin/ps -ef");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

END_TEST






TCase *rsys_rsystemd_tests(void)
{
TCase *tc_rsystemd;

  printf("%s: %d\n", __FUNCTION__, getpid());

  /* Error test case */
  tc_rsystemd = tcase_create("RSYSTEMD command");

  // Increase the timeout for the test case as we use lots of timeouts
  tcase_set_timeout(tc_rsystemd, 60);

  tcase_add_unchecked_fixture(tc_rsystemd, rsys_setup_unchecked_fixture, 0);
  tcase_add_checked_fixture(tc_rsystemd, rsys_setup_checked_fixture, 0);
  tcase_add_test(tc_rsystemd, test_rsystemd);
  tcase_add_test(tc_rsystemd, test_example);
  tcase_add_test(tc_rsystemd, test_debug);

  return tc_rsystemd;
} // rsys_rsystemd_tests
