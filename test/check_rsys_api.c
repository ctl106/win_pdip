// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : check_rsys_api.c
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


#include <errno.h>
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
START_TEST(test_rsystem)

int   status, status1, status2, status3;
int   rc, rc1, rc2, rc3;
char *cmd;

  // Empty command
  status = rsystem("%s", "");
  ck_assert_int_eq(status, 0);

  // Command line which triggers a dynamic allocation
  cmd = (char *)malloc(5000);
  ck_assert_ptr_ne(cmd, 0);
  memset(cmd, ' ', 5000);
  cmd[0] = '#';
  cmd[4999] = '\0';
  status = rsystem("%s", cmd);
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);
  free(cmd);

  status = rsystem("/qwerty/azerty/cmd_not_exist");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 127);

  status = rsystem("%s", "/bin/date");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

  status = rsystem("%s", "/bin/date --bad_param");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  status = rsystem("%s", "test/pterm -e 4");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 4);

  status = rsystem("%s", "test/pterm -k 15");
  ck_assert(WIFSIGNALED(status));
  ck_assert_int_eq(WTERMSIG(status), 15);

  status = rsystem("%s", "test/pterm -k 9");
  ck_assert(WIFSIGNALED(status));
  ck_assert_int_eq(WTERMSIG(status), 9);

  // Program which does not display anything
  status = rsystem("%s", "/bin/sleep 1");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

  //
  // Fork a child into which PDIP/RSYS are deactivated by default
  //
  rc = check_fork();
  ck_assert_int_ne(rc, -1);
  if (rc)
  {
    // Father
    status =  rsys_wait_child(rc);
    ck_assert(WIFEXITED(status));
    ck_assert_int_eq(WEXITSTATUS(status), 0);
  }
  else // Child
  {
    status = rsystem("%s", "/bin/sleep 1");
    ck_assert_int_eq(status, -1);
    exit(0);
  }



  //
  // Fork a child into which PDIP/RSYS are activated but no command are
  // launched before exit
  //
  rc = check_fork();
  ck_assert_int_ne(rc, -1);
  if (rc)
  {
    // Father
    status =  rsys_wait_child(rc);
    ck_assert(WIFEXITED(status));
    ck_assert_int_eq(WEXITSTATUS(status), 0);
  }
  else // Child
  {
    rc = rsys_lib_initialize();
    ck_assert_int_eq(rc, 0);

    // No command

    exit(0);
  }


  //
  // Multiple commands
  //
  status = rsystem("%s; %s; %s", "/bin/echo tyty", "/bin/sleep 1", "/bin/date");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

  // Very verbose program
  status = rsystem("%s", "test/pdata -B \"VERBOSE PROGRAM\" -b 1055 -l 117 -T \"END OF VERBOSE PROGRAM\"");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

  //
  // Command line ended with line feeds and spaces
  //
  status = rsystem("%s", "/bin/date;/bin/sleep 1 \n \n\n \f \r \v \t");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);


  //
  // Trigger a busy situation where we launch two commands at the same time on the
  // server#0 which has only one running shell
  //
  rc1 = ck_rsys_child_rsystem("/bin/sleep 2", 0);
  ck_assert_int_gt(rc1, 0);
  rc2 = ck_rsys_child_rsystem("/bin/sleep 2", 0);
  ck_assert_int_gt(rc2, 0);

  status1 = rsys_wait_child(rc1);
  ck_assert(WIFEXITED(status1));
  status2 = rsys_wait_child(rc2);
  ck_assert(WIFEXITED(status2));

  if (0 == WEXITSTATUS(status1))
  {
    ck_assert_int_eq(WEXITSTATUS(status2), 1);
  }
  else
  {
    ck_assert_int_eq(WEXITSTATUS(status1), 1);
    ck_assert_int_eq(WEXITSTATUS(status2), 0);
  }


  //
  // 3 commands in parallel for 3 shells on server side
  //
  rc1 = ck_rsys_child_rsystem("/bin/sleep 2; echo end of cmd1#$$", 4);
  ck_assert_int_gt(rc1, 0);

  rc2 = ck_rsys_child_rsystem("/bin/sleep 2; echo end of cmd2#$$", 4);
  ck_assert_int_gt(rc2, 0);

  rc3 = ck_rsys_child_rsystem("/bin/sleep 2; echo end of cmd3#$$", 4);
  ck_assert_int_gt(rc3, 0);
  
  status1 = rsys_wait_child(rc1);
  ck_assert(WIFEXITED(status1));
  ck_assert_int_eq(WEXITSTATUS(status1), 0);

  status2 = rsys_wait_child(rc2);
  ck_assert(WIFEXITED(status2));
  ck_assert_int_eq(WEXITSTATUS(status2), 0);

  status3 = rsys_wait_child(rc3);
  ck_assert(WIFEXITED(status3));
  ck_assert_int_eq(WEXITSTATUS(status3), 0);

END_TEST



TCase *rsys_api_tests(void)
{
TCase *tc_api;

  printf("%s: %d\n", __FUNCTION__, getpid());

  /* Error test case */
  tc_api = tcase_create("RSYS API");

  // Increase the timeout for the test case as we use lots of timeouts
  tcase_set_timeout(tc_api, 60);

  tcase_add_unchecked_fixture(tc_api, rsys_setup_unchecked_fixture, 0);
  tcase_add_checked_fixture(tc_api, rsys_setup_checked_fixture, 0);
  tcase_add_test(tc_api, test_rsystem);

  return tc_api;
} // rsys_api_tests
