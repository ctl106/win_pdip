// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : check_pdip_errcodes.c
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


#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#include "check_all.h"
#include "check_pdip.h"
#include "../pdip.h"


// This is run once in the main process before each test cases
static void pdip_setup_unchecked_fixture(void)
{

  printf("%s: %d\n", __FUNCTION__, getpid());

} // pdip_setup_unchecked_fixture


// This is run once inside each test case processes
static void pdip_setup_checked_fixture(void)
{

  printf("%s: %d\n", __FUNCTION__, getpid());

  // As the test cases are run in child processes, PDIP service
  // must be initialized inside them
  (void)pdip_lib_initialize();

} // pdip_setup_checked_fixture




// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_cfg_init_err)

int rc;

  printf("%s: %d\n", __FUNCTION__, getpid());

  rc = pdip_cfg_init(NULL);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EINVAL);

END_TEST




// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_new_err)

int    rc;
pdip_t pdip_1;

  printf("%s: %d\n", __FUNCTION__, getpid());

  // No error branches in pdip_new() except the memory allocation pbs

  pdip_1 = pdip_new((pdip_cfg_t *)0);
  ck_assert(pdip_1 != NULL);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

END_TEST


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_delete_err)

int    rc;

  printf("%s: %d\n", __FUNCTION__, getpid());

  rc = pdip_delete(NULL, NULL);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EINVAL);

  rc = pdip_delete((pdip_t)0x1, NULL);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(ENOENT);

END_TEST


// Signal handler for SIGCHLD
static void tpdip_sigchld_hdl(int sig, siginfo_t *info, void *p)
{
int rc;

  (void)p;

  printf("SIGCHLD for process %d\n", info->si_pid);

  rc = pdip_signal_handler(sig, info);
  ck_assert_int_eq(rc, PDIP_SIG_HANDLED);

} // pdip_internal_sig_hdl


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_exec_err)

int               rc;
char             *av[10];
pdip_t            pdip_1;
struct sigaction  action;
int               status;

  printf("%s: %d\n", __FUNCTION__, getpid());

  // Register SIGCHLD signal handler
  action.sa_sigaction = tpdip_sigchld_hdl;
  sigemptyset(&(action.sa_mask));
  action.sa_flags     = SA_SIGINFO;
  rc = sigaction(SIGCHLD, &action, 0);
  ck_assert_int_eq(rc, 0);

  rc = pdip_configure(0, 0);
  ck_assert_int_eq(rc, 0);

  rc = pdip_exec(NULL, 0, NULL);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EINVAL);

  rc = pdip_exec(NULL, -1, NULL);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EINVAL);

  av[0] = NULL;
  rc = pdip_exec(NULL, 1, av);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EINVAL);

  av[0] = "/bin/sh";
  av[1] = NULL;
  rc = pdip_exec(NULL, 2, av);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EINVAL);

  av[0] = "/bin/sh";
  av[1] = "toto";
  av[2] = "tyty";
  rc = pdip_exec(NULL, 2, av);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EINVAL);

  pdip_1 = pdip_new((pdip_cfg_t *)0);
  ck_assert(pdip_1 != NULL);

  //pdip_set_debug_level(pdip_1, 20);

  av[0] = "/tyty/toto/foo";
  av[1] = NULL;
  rc = pdip_exec(pdip_1, 1, av);
  ck_assert_int_gt(rc, 1);

  // Wait for the end of the child process
  rc = pdip_status(pdip_1, &status, 1);
  ck_assert_int_eq(rc, 0);
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);


  // Attach a process to an object already attached to a process

  pdip_1 = pdip_new((pdip_cfg_t *)0);
  ck_assert(pdip_1 != NULL);

  av[0] = "/bin/sleep";
  av[1] = "2";
  av[2] = NULL;
  rc = pdip_exec(pdip_1, 2, av);
  ck_assert_int_gt(rc, 0);

  av[0] = "/bin/sh";
  av[1] = NULL;
  rc = pdip_exec(pdip_1, 1, av);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EPERM);

  // Wait for the end of the controlled process
  rc = pdip_status(pdip_1, &status, 1);
  ck_assert_int_eq(rc, 0);
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);
  
  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

END_TEST


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_send_err)

int               rc;
pdip_t            pdip_1;
struct sigaction  action;
char             *av[10];
int               status;
char             *buf;
unsigned int      i;
char             *display;
size_t            display_sz;
size_t            data_sz;
struct timeval    timeout;
pdip_cfg_t        cfg;

  display_sz = 0;
  display = (char *)0;

  printf("%s: %d\n", __FUNCTION__, getpid());

  rc = pdip_send(NULL, NULL);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EINVAL);

  rc = pdip_send((pdip_t)0x1, NULL);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EINVAL);


  // Register SIGCHLD signal handler
  action.sa_sigaction = tpdip_sigchld_hdl;
  sigemptyset(&(action.sa_mask));
  action.sa_flags     = SA_SIGINFO;
  rc = sigaction(SIGCHLD, &action, 0);
  ck_assert_int_eq(rc, 0);

  rc = pdip_configure(0, 0);
  ck_assert_int_eq(rc, 0);


  pdip_1 = pdip_new((pdip_cfg_t *)0);
  ck_assert(pdip_1 != NULL);

  av[0] = "/bin/sh";
  av[1] = NULL;
  rc = pdip_exec(pdip_1, 1, av);
  ck_assert_int_gt(rc, 1);

  //pdip_set_debug_level(pdip_1, 5);

  rc = pdip_sig(pdip_1, SIGKILL);
  ck_assert_int_eq(rc, 0);

  // Wait for the end of the controlled process
  rc = pdip_status(pdip_1, &status, 1);
  ck_assert_int_eq(rc, 0);
  ck_assert(WIFSIGNALED(status));
  ck_assert_int_eq(WTERMSIG(status), SIGKILL);  

  rc = pdip_send(pdip_1, "toto");
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EPERM);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

  //
  // Format too big for internal buffers
  //
#define CK_PDIP_BUF_SIZE 5000
  buf = (char *)malloc(CK_PDIP_BUF_SIZE);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < (CK_PDIP_BUF_SIZE - 1); i ++)
  {
    buf[i] = 'A' + (i % 26);
  } // End for
  buf[i] = '\0';

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags = PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);
  
  pdip_set_debug_level(pdip_1, 10);

  rc = setenv("PS1", CK_PDIP_PROMPT, 1);
  ck_assert_int_eq(rc, 0);

  av[0] = "/bin/sh";
  av[1] = NULL;
  rc = pdip_exec(pdip_1, 1, av);
  ck_assert_int_gt(rc, 1);

  // Wait for the prompt to make sure that the program is started
  data_sz = 0;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^" CK_PDIP_PROMPT "$", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_FOUND);
  ck_assert_uint_gt(data_sz, 0);

  rc = pdip_send(pdip_1, "Format too long: '%s'\n", buf);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(ENOSPC);

  free(buf);
  free(display);
  display_sz = 0;
  display = (char *)0;  

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

END_TEST


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_sig_err)

int               rc;
pdip_t            pdip_1;
struct sigaction  action;

  // Register SIGCHLD signal handler
  action.sa_sigaction = tpdip_sigchld_hdl;
  sigemptyset(&(action.sa_mask));
  action.sa_flags     = SA_SIGINFO;
  rc = sigaction(SIGCHLD, &action, 0);
  ck_assert_int_eq(rc, 0);

  rc = pdip_sig(0, SIGTERM);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EINVAL);

  pdip_1 = pdip_new((pdip_cfg_t *)0);
  ck_assert(pdip_1 != NULL);

  rc = pdip_sig(pdip_1, SIGTERM);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EPERM);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

END_TEST


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_flush_err)

int               rc;
pdip_t            pdip_1;
char             *display;
size_t            display_sz;
size_t            data_sz;

  rc = pdip_flush(0, 0, 0, 0);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EINVAL);
 
  pdip_1 = pdip_new((pdip_cfg_t *)0);
  ck_assert(pdip_1 != NULL);

  rc = pdip_flush(pdip_1, 0, 0, 0);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EINVAL);

  rc = pdip_flush(pdip_1, &display, 0, 0);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EINVAL);

  rc = pdip_flush(pdip_1, &display, &display_sz, 0);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EINVAL);

  rc = pdip_flush(pdip_1, &display, &display_sz, &data_sz);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EPERM);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

END_TEST



// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_status_err)

int               rc;
pdip_t            pdip_1;
char             *av[2];
pdip_cfg_t        cfg;
char             *display;
size_t            display_sz;
size_t            data_sz;
struct timeval    timeout;

  display_sz = 0;
  display = (char *)0;

  // To make sure that SIGCHLD will be managed by the service
  rc = pdip_configure(1, 0);
  ck_assert_int_eq(rc, 0);

  rc = pdip_status(0, 0, 0);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EINVAL);

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags = PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  rc = pdip_status(pdip_1, 0, 0);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EPERM);

  rc = setenv("PS1", "TST> ", 1);
  ck_assert_int_eq(rc, 0);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "/bin/sh";
  av[1] = NULL;
  rc = pdip_exec(pdip_1, 1, av);
  ck_assert_int_gt(rc, 1);

  rc = pdip_status(pdip_1, 0, 0);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EAGAIN);

  // Wait for the prompt to make sure that the program is started before killing it
  // otherwise CHECK complains about signals
  data_sz = 0;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^" "TST> " "$", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_FOUND);
  ck_assert_uint_gt(data_sz, 0);

  free(display);
  display_sz = 0;
  display = (char *)0;

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

END_TEST


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_recv_err)

int               rc;
pdip_t            pdip_1;
char             *display;
size_t            display_sz;
size_t            data_sz;
char             *av[2];

  rc = pdip_recv(0, 0, 0, 0, 0, 0);
  ck_assert_int_eq(rc, PDIP_RECV_ERROR);
  ck_assert_errno_eq(EINVAL);

  rc = pdip_recv(0, 0, 0, 0, &data_sz, 0);
  ck_assert_int_eq(rc, PDIP_RECV_ERROR);
  ck_assert_errno_eq(EINVAL);

  rc = pdip_recv(0, 0, 0, &display_sz, &data_sz, 0);
  ck_assert_int_eq(rc, PDIP_RECV_ERROR);
  ck_assert_errno_eq(EINVAL);

  rc = pdip_recv(0, 0, &display, &display_sz, &data_sz, 0);
  ck_assert_int_eq(rc, PDIP_RECV_ERROR);
  ck_assert_errno_eq(EINVAL);

  pdip_1 = pdip_new((pdip_cfg_t *)0);
  ck_assert(pdip_1 != NULL);

  //pdip_set_debug_level(pdip_1, 20);

  display_sz = 10;
  display = (char *)0;
  rc = pdip_recv(pdip_1, 0, &display, &display_sz, &data_sz, 0);
  ck_assert_int_eq(rc, PDIP_RECV_ERROR);
  ck_assert_errno_eq(EINVAL);

  display_sz = 0;
  display = (char *)0x8;
  rc = pdip_recv(pdip_1, 0, &display, &display_sz, &data_sz, 0);
  ck_assert_int_eq(rc, PDIP_RECV_ERROR);
  ck_assert_errno_eq(EINVAL);

  // No controlled program
  display_sz = 0;
  data_sz = 0;
  display = (char *)0;
  rc = pdip_recv(pdip_1, "$.|*", &display, &display_sz, &data_sz, 0);
  ck_assert_int_eq(rc, PDIP_RECV_ERROR);
  ck_assert_errno_eq(EPERM);

  // Bad regex
  av[0] = "/bin/sh";
  av[1] = NULL;
  rc = pdip_exec(pdip_1, 1, av);
  ck_assert_int_gt(rc, 1);

  display_sz = 0;
  data_sz = 0;
  display = (char *)0;
  rc = pdip_recv(pdip_1, "$.|*", &display, &display_sz, &data_sz, 0);
  ck_assert_int_eq(rc, PDIP_RECV_ERROR);
  ck_assert_errno_eq(EINVAL);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

END_TEST


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_fd_err)

int               rc;
pdip_t            pdip_1;

  rc = pdip_fd(0);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EINVAL);

  pdip_1 = pdip_new((pdip_cfg_t *)0);
  ck_assert(pdip_1 != NULL);

  rc = pdip_fd(pdip_1);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EPERM);

END_TEST


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_term_settings_err)

pdip_t            pdip_1;

extern void pdip_term_settings(pdip_t ctx);

  pdip_1 = pdip_new((pdip_cfg_t *)0);
  ck_assert(pdip_1 != NULL);

  pdip_term_settings(pdip_1);

END_TEST


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_cpu_free_err)

int rc;

 rc = pdip_cpu_free(0);
 ck_assert_int_eq(rc, -1);
 ck_assert_errno_eq(EINVAL); 

END_TEST



// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_cpu_zero_err)

int rc;

 rc = pdip_cpu_zero(0);
 ck_assert_int_eq(rc, -1);
 ck_assert_errno_eq(EINVAL); 

END_TEST


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_cpu_all_err)

int rc;

 rc = pdip_cpu_all(0);
 ck_assert_int_eq(rc, -1);
 ck_assert_errno_eq(EINVAL); 

END_TEST


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_cpu_isset_err)

int rc;

 rc = pdip_cpu_isset(0, pdip_cpu_nb());
 ck_assert_int_eq(rc, -1);
 ck_assert_errno_eq(EINVAL); 

 rc = pdip_cpu_isset(0, pdip_cpu_nb() + 2);
 ck_assert_int_eq(rc, -1);
 ck_assert_errno_eq(EINVAL); 

END_TEST


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_cpu_set_err)

int rc;

 rc = pdip_cpu_set(0, pdip_cpu_nb());
 ck_assert_int_eq(rc, -1);
 ck_assert_errno_eq(EINVAL); 

 rc = pdip_cpu_set(0, pdip_cpu_nb() + 2);
 ck_assert_int_eq(rc, -1);
 ck_assert_errno_eq(EINVAL); 

END_TEST


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_cpu_unset_err)

int rc;

 rc = pdip_cpu_unset(0, pdip_cpu_nb());
 ck_assert_int_eq(rc, -1);
 ck_assert_errno_eq(EINVAL); 

 rc = pdip_cpu_unset(0, pdip_cpu_nb() + 2);
 ck_assert_int_eq(rc, -1);
 ck_assert_errno_eq(EINVAL); 

END_TEST




// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_signal_handler_err)

int       rc;
siginfo_t info;

  rc = pdip_signal_handler(SIGCHLD, 0);
  ck_assert_int_eq(rc, PDIP_SIG_ERROR);

  rc = pdip_signal_handler(SIGKILL, &info);
  ck_assert_int_eq(rc, PDIP_SIG_UNKNOWN);

  // Unknown process
  pdip_set_debug_level(0, 5);
  info.si_pid = 1;
  rc = pdip_signal_handler(SIGCHLD, &info);
  ck_assert_int_eq(rc, PDIP_SIG_ERROR);

END_TEST



TCase *pdip_errcodes_tests(void)
{
TCase *tc_err_code;

  printf("%s: %d\n", __FUNCTION__, getpid());

  /* Error test case */
  tc_err_code = tcase_create("PDIP error codes");

  // Increase the timeout for the test case as we use lots of timeouts
  tcase_set_timeout(tc_err_code, 60);

  tcase_add_unchecked_fixture(tc_err_code, pdip_setup_unchecked_fixture, 0);
  tcase_add_checked_fixture(tc_err_code, pdip_setup_checked_fixture, 0);
  tcase_add_test(tc_err_code, test_pdip_cfg_init_err);
  tcase_add_test(tc_err_code, test_pdip_new_err);
  tcase_add_test(tc_err_code, test_pdip_delete_err);
  tcase_add_test(tc_err_code, test_pdip_exec_err);
  tcase_add_test(tc_err_code, test_pdip_send_err);
  tcase_add_test(tc_err_code, test_pdip_sig_err);
  tcase_add_test(tc_err_code, test_pdip_flush_err);
  tcase_add_test(tc_err_code, test_pdip_status_err);
  //tcase_add_test(tc_err_code, test_pdip_recv_err);
  tcase_add_test_raise_signal(tc_err_code, test_pdip_recv_err, SIGTERM);
  tcase_add_test(tc_err_code, test_pdip_fd_err);
  tcase_add_test(tc_err_code, test_pdip_term_settings_err);
  tcase_add_test(tc_err_code, test_pdip_cpu_free_err);
  tcase_add_test(tc_err_code, test_pdip_cpu_zero_err);
  tcase_add_test(tc_err_code, test_pdip_cpu_all_err);
  tcase_add_test(tc_err_code, test_pdip_cpu_isset_err);
  tcase_add_test(tc_err_code, test_pdip_cpu_set_err);
  tcase_add_test(tc_err_code, test_pdip_cpu_unset_err);
  tcase_add_test(tc_err_code, test_pdip_signal_handler_err);

  return tc_err_code;
} // pdip_errcodes_tests
