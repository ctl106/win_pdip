// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : check_pdip_api.c
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
//     30-Jan-2018  R. Koucha    - Creation
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <malloc.h>

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
START_TEST(test_pdip_recv)

int               rc;
pdip_t            pdip_1;
char             *display;
size_t            display_sz;
size_t            data_sz;
char             *av[6];
struct timeval    timeout;
pdip_cfg_t        cfg;
int               status;

  display_sz = 0;
  display = (char *)0;

  rc = pdip_configure(1, 0);
  ck_assert_int_eq(rc, 0);

  //
  // As the shell sends its prompt on the standard error,
  // It will not send anything in the PDIP terminal if we don't
  // redirect its standard error into it
  //
  pdip_1 = pdip_new((pdip_cfg_t *)0);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "/bin/sh";
  av[1] = NULL;
  rc = pdip_exec(pdip_1, 1, av);
  ck_assert_int_gt(rc, 1);

  //
  // Timeout without regex
  //
  data_sz = 0;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, 0, &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_TIMEOUT);

  //
  // Timeout with regex
  //
  data_sz = 0;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "> toto$", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_TIMEOUT);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

  //
  // Receive outstanding data
  //

  //
  // As the shell sends its prompt on the standard error,
  // we need to redirect its standard error to the PDIP terminal
  // as we will use the shell prompt as outstanding data for the
  // following tests
  //
  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags = PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "/bin/sh";
  av[1] = NULL;
  rc = pdip_exec(pdip_1, 1, av);
  ck_assert_int_gt(rc, 1);

  // Wait for a regex which will not be found
  // ==> The prompt of the shell will go inside the outstanding data
  data_sz = 0;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^qwerty =$", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_TIMEOUT);
  ck_assert_uint_eq(data_sz, 0);

  // Wait without regex to get outstanding data (shell prompt)
  data_sz = 0;
  rc = pdip_recv(pdip_1, 0, &display, &display_sz, &data_sz, 0);
  ck_assert_int_eq(rc, PDIP_RECV_DATA);
  ck_assert_uint_gt(data_sz, 0);
  ck_assert_uint_gt(display_sz, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);


  // Wait with regex

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags = PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  rc = setenv("PS1", CK_PDIP_PROMPT, 1);
  ck_assert_int_eq(rc, 0);

  av[0] = "/bin/sh";
  av[1] = NULL;
  rc = pdip_exec(pdip_1, 1, av);
  ck_assert_int_gt(rc, 1);

  // Wait for the prompt
  data_sz = 0;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^" CK_PDIP_PROMPT "$", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_FOUND);
  ck_assert_uint_gt(data_sz, 0);
  ck_assert_uint_gt(display_sz, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);


  // Infinite read on a process which finishes

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "/bin/sleep";
  av[1] = "1";
  av[2] = NULL;
  rc = pdip_exec(pdip_1, 2, av);
  ck_assert_int_gt(rc, 1);

  // Wait for data
  data_sz = 0;
  rc = pdip_recv(pdip_1, 0, &display, &display_sz, &data_sz, 0);
  ck_assert_int_eq(rc, PDIP_RECV_ERROR);
  ck_assert_uint_eq(data_sz, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);


  // Read with timeout on a process which finishes before the timeout

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "/bin/sleep";
  av[1] = "1";
  av[2] = NULL;
  rc = pdip_exec(pdip_1, 2, av);
  ck_assert_int_gt(rc, 1);

  // Wait for data
  data_sz = 0;
  timeout.tv_sec = 4;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, 0, &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_ERROR);
  ck_assert_uint_eq(data_sz, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);



  //
  // Receive data with no regex, no timeout and buffer = NULL
  //

  // Free buffer
  if (display_sz)
  {
    free(display);
    display = (char *)0;
    display_sz = 0;
  }

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "test/pdata";
  av[1] = "-k 1";
  av[2] = NULL;
  rc = pdip_exec(pdip_1, 2, av);
  ck_assert_int_gt(rc, 1);

  // Wait for data
  data_sz = 0;
  rc = pdip_recv(pdip_1, 0, &display, &display_sz, &data_sz, 0);
  ck_assert_int_eq(rc, PDIP_RECV_DATA);
  ck_assert_uint_gt(data_sz, 0);
  ck_assert_uint_gt(display_sz, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);


  //
  // Receive data with no regex, no timeout and buffer size < default increment (1024)
  //

  // Realloc buffer
  display_sz = 256;
  display = realloc(display, display_sz);
  ck_assert(display);

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "test/pdata";
  av[1] = "-k 1";
  av[2] = NULL;
  rc = pdip_exec(pdip_1, 2, av);
  ck_assert_int_gt(rc, 1);

  // Wait for data
  data_sz = 0;
  rc = pdip_recv(pdip_1, 0, &display, &display_sz, &data_sz, 0);
  ck_assert_int_eq(rc, PDIP_RECV_DATA);
  ck_assert_uint_gt(data_sz, 0);
  ck_assert_uint_gt(display_sz, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);


  //
  // Receive data with no regex, no timeout and buffer size > default increment (1024)
  //

  // Realloc buffer
  display_sz = 4096;
  display = realloc(display, display_sz);
  ck_assert(display);

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "test/pdata";
  av[1] = "-k 1";
  av[2] = NULL;
  rc = pdip_exec(pdip_1, 2, av);
  ck_assert_int_gt(rc, 1);

  // Wait for data
  data_sz = 0;
  rc = pdip_recv(pdip_1, 0, &display, &display_sz, &data_sz, 0);
  ck_assert_int_eq(rc, PDIP_RECV_DATA);
  ck_assert_uint_gt(data_sz, 0);
  ck_assert_uint_gt(display_sz, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);



  //
  // Receive data with regex, no timeout (PDIP_FLAG_RECV_ON_THE_FLOW)
  //

  // Free buffer
  if (display_sz)
  {
    free(display);
    display = (char *)0;
    display_sz = 0;
  }

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags |= PDIP_FLAG_RECV_ON_THE_FLOW;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "test/pdata";
  av[1] = "-k 1";
  av[2] = NULL;
  rc = pdip_exec(pdip_1, 2, av);
  ck_assert_int_gt(rc, 1);

  // Wait for data with a regex which will not be found
  data_sz = 0;
  rc = pdip_recv(pdip_1, "^qwerty", &display, &display_sz, &data_sz, 0);
  ck_assert_int_eq(rc, PDIP_RECV_DATA);
  ck_assert_uint_eq(data_sz, 1024 + 1);
  ck_assert_uint_gt(display_sz, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);



  //
  // Receive data with regex, timeout (PDIP_FLAG_RECV_ON_THE_FLOW)
  //

  // Free buffer
  if (display_sz)
  {
    free(display);
    display = (char *)0;
    display_sz = 0;
  }

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags |= PDIP_FLAG_RECV_ON_THE_FLOW;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "test/pdata";
  av[1] = "-k 1";
  av[2] = "-l 4";
  av[3] = "-f";
  av[4] = NULL;
  rc = pdip_exec(pdip_1, 4, av);
  ck_assert_int_gt(rc, 1);

  // Wait for data with a regex which will not be found
  data_sz = 0;
  do
  {
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    rc = pdip_recv(pdip_1, "^qwerty", &display, &display_sz, &data_sz, &timeout);
    switch(rc)
    {
      case PDIP_RECV_DATA :
      {
        ck_assert_uint_gt(data_sz, 0);
        ck_assert_uint_gt(display_sz, 0);
      }
      break;

      case PDIP_RECV_TIMEOUT :
      {
      }
      break;

      default:
      {
        // To display the value of "rc" in the error message
        ck_assert_int_eq(rc, PDIP_RECV_TIMEOUT);
      }
    }
  } while(rc != PDIP_RECV_TIMEOUT);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);



  //
  // Receive data with regex, timeout, no data from process
  //

  // Free buffer
  if (display_sz)
  {
    free(display);
    display = (char *)0;
    display_sz = 0;
  }

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "test/pdata";
  av[1] = "-k 0";
  av[2] = "-l 0";
  av[3] = "-f";
  av[4] = NULL;
  rc = pdip_exec(pdip_1, 4, av);
  ck_assert_int_gt(rc, 1);

  // Wait for data with a regex which will not be found
  data_sz = 0;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^qwerty", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_TIMEOUT);
  ck_assert_uint_eq(data_sz, 0);
  ck_assert_uint_eq(display_sz, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);


  //
  // Receive data with regex, timeout, some data from process
  //

  // Free buffer
  if (display_sz)
  {
    free(display);
    display = (char *)0;
    display_sz = 0;
  }

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "test/pdata";
  av[1] = "-k 1";
  av[2] = "-l 1";
  av[3] = "-f";
  av[4] = NULL;
  rc = pdip_exec(pdip_1, 4, av);
  ck_assert_int_gt(rc, 1);

  // Wait for data with a regex which will not be found
  data_sz = 0;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^qwerty", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_TIMEOUT);
  ck_assert_uint_eq(data_sz, 0);
  ck_assert_uint_eq(display_sz, 0);

  // There are outstanding data in the object. Receive one part with a regular expression.
  rc = pdip_recv(pdip_1, "zabcd", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_FOUND);
  ck_assert_uint_gt(data_sz, 0);
  ck_assert_uint_gt(display_sz, 0);

  // Receive all the remaining outstanding data
  rc = pdip_recv(pdip_1, 0, &display, &display_sz, &data_sz, 0);
  ck_assert_int_eq(rc, PDIP_RECV_DATA);
  ck_assert_uint_gt(data_sz, 0);
  ck_assert_uint_gt(display_sz, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);


  //
  // Receive data with regex, timeout, some data from process with no matching
  //

  // Free buffer
  if (display_sz)
  {
    free(display);
    display = (char *)0;
    display_sz = 0;
  }

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "test/pdata";
  av[1] = "-k 1";
  av[2] = "-l 1";
  av[3] = "-f";
  av[4] = NULL;
  rc = pdip_exec(pdip_1, 4, av);
  ck_assert_int_gt(rc, 1);

  // Wait for data with a regex which will not be found
  data_sz = 0;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^qwerty", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_TIMEOUT);
  ck_assert_uint_eq(data_sz, 0);
  ck_assert_uint_eq(display_sz, 0);

  // Receive the outstanding data
  rc = pdip_recv(pdip_1, 0, &display, &display_sz, &data_sz, 0);
  ck_assert_int_eq(rc, PDIP_RECV_DATA);
  ck_assert_uint_eq(data_sz, 1025);
  ck_assert_uint_gt(display_sz, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);


  //
  // Receive data with regex which matches begining of line
  //

  // Free buffer
  if (display_sz)
  {
    free(display);
    display = (char *)0;
    display_sz = 0;
  }

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "test/pdata";
  av[1] = "-k 1";
  av[2] = "-l 1";
  av[3] = "-f";
  av[4] = NULL;
  rc = pdip_exec(pdip_1, 4, av);
  ck_assert_int_gt(rc, 1);

  // Wait for data with regex matching beginning of line
  data_sz = 0;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_FOUND);
  ck_assert_uint_eq(data_sz, 0);

  // Wait for data with regex matching end of line
  data_sz = 0;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "$", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_FOUND);
  ck_assert_uint_gt(data_sz, 0);
  ck_assert_uint_gt(display_sz, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);


  //
  // Receive data with regex which matches begining of line with an interactive process
  // which does not print prompts (like "bc" calculator)
  //

  // Free buffer
  if (display_sz)
  {
    free(display);
    display = (char *)0;
    display_sz = 0;
  }

  // The prompt is displayed on stderr (no need for terminating '\n' to be printed)
  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags |= PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "test/pdata";
  av[1] = "-B";
  av[2] = "Program's banner";
  av[3] = "-i";
  av[4] = "";
  av[5] = NULL;
  rc = pdip_exec(pdip_1, 5, av);
  ck_assert_int_gt(rc, 1);

  // Wait for the begining of the banner
  data_sz = 0;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_FOUND);
  ck_assert_uint_eq(data_sz, 0);

  // Wait for the end of the banner
  data_sz = 0;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "$", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_FOUND);
  ck_assert_uint_gt(data_sz, 0);

  // Send some data
  rc = pdip_send(pdip_1, "azerty\n");
  ck_assert_int_gt(rc, 0);

  // Wait for echoed data with regex matching end of line
  data_sz = 0;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "$", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_FOUND);
  ck_assert_uint_gt(data_sz, 0);

  fprintf(stderr, "Received data=<%s>\n", display);

  // Send EOF (CTRL-D)
  rc = pdip_send(pdip_1, "%c", 0x04);
  ck_assert_int_gt(rc, 0);

  rc = pdip_status(pdip_1, &status, 1);
  ck_assert_int_eq(rc, 0);
  ck_assert_int_eq(status, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);



  //
  // Receive data with regex which does not match the prompt in a
  // first step then matching the prompt
  // ==> This simulates the case where we look for a match but the last
  // received line is not complete (no terminating '\n')
  //

  // Free buffer
  if (display_sz)
  {
    free(display);
    display = (char *)0;
    display_sz = 0;
  }

  // The prompt is displayed on stderr (no need for terminating '\n' to be printed)
  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags |= (PDIP_FLAG_ERR_REDIRECT | PDIP_FLAG_RECV_ON_THE_FLOW);
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "test/pdata";
  av[1] = "-B";
  av[2] = "Program's banner";
  av[3] = "-i";
  av[4] = "prt> ";
  av[5] = NULL;
  rc = pdip_exec(pdip_1, 5, av);
  ck_assert_int_gt(rc, 1);

  // Wait for an inexisting string, the service returns the complete lines (banner) before the incomplete line (the prompt)
  data_sz = 0;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "_impossible_", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_DATA);
  ck_assert_uint_eq(data_sz, 17); // "banner\n"

  fprintf(stderr, "Received data _impossible_ =<%s>\n", display);

  // Receive the prompt
  data_sz = 0;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^prt> ", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_FOUND);
  ck_assert_uint_eq(data_sz, 5); // "prt> "

  fprintf(stderr, "Received data prompt =<%s>\n", display);

  // Send some data
  rc = pdip_send(pdip_1, "azerty\n");
  ck_assert_int_gt(rc, 0);

  // Wait for echoed data with regex matching end of line
  data_sz = 0;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "$", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_FOUND);
  ck_assert_uint_gt(data_sz, 0);

  fprintf(stderr, "Received data=<%s>\n", display);

  // Send EOF (CTRL-D)
  rc = pdip_send(pdip_1, "%c", 0x04);
  ck_assert_int_gt(rc, 0);

  rc = pdip_status(pdip_1, &status, 1);
  ck_assert_int_eq(rc, 0);
  ck_assert_int_eq(status, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

END_TEST



// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_send)

int               rc;
pdip_t            pdip_1;
char             *display;
size_t            display_sz;
size_t            data_sz;
char             *av[2];
struct timeval    timeout;
pdip_cfg_t        cfg;

  display_sz = 0;
  display = (char *)0;

  rc = pdip_configure(1, 0);
  ck_assert_int_eq(rc, 0);

  //
  // As the shell sends its prompt on the standard error,
  // It will not send anything in the PDIP terminal if we don't
  // redirect its standard error into it
  //
#define CK_PDIP_PROMPT "PRompt> "

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags = PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  rc = setenv("PS1", CK_PDIP_PROMPT, 1);
  ck_assert_int_eq(rc, 0);

  av[0] = "/bin/sh";
  av[1] = NULL;
  rc = pdip_exec(pdip_1, 1, av);
  ck_assert_int_gt(rc, 1);

  // Wait for the prompt
  data_sz = 0;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^" CK_PDIP_PROMPT "$", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_FOUND);
  ck_assert_uint_gt(data_sz, 0);

  rc = pdip_send(pdip_1, "%s\n", "ls -la");
  ck_assert_int_gt(rc, 0);

  // Wait for the prompt
  data_sz = 0;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^" CK_PDIP_PROMPT "$", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_FOUND);
  ck_assert_uint_gt(data_sz, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);


END_TEST




// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_flush)

int               rc;
pdip_t            pdip_1;
char             *display;
size_t            display_sz;
size_t            data_sz;
char             *av[2];
struct timeval    timeout;
pdip_cfg_t        cfg;

  display_sz = 0;
  display = (char *)0;

  rc = pdip_configure(1, 0);
  ck_assert_int_eq(rc, 0);

  //
  // Receive outstanding data
  //

  //
  // As the shell sends its prompt on the standard error,
  // we need to redirect its standard error to the PDIP terminal
  // as we will use the shell prompt as outstanding data for the
  // following tests
  //
  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags = PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "/bin/sh";
  av[1] = NULL;
  rc = pdip_exec(pdip_1, 1, av);
  ck_assert_int_gt(rc, 1);

  // Wait for a regex which will not be found
  // ==> The prompt of the shell will go inside the outstanding data
  data_sz = 0;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^qwerty =$", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_TIMEOUT);
  ck_assert_uint_eq(data_sz, 0);

  // Flush outstanding data
  data_sz = 0;
  rc = pdip_flush(pdip_1, &display, &display_sz, &data_sz);
  ck_assert_int_eq(rc, 0);
  ck_assert_uint_gt(data_sz, 0);


  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);


  //
  // Receive outstanding data with allocated receiving buffer
  //

  //
  // As the shell sends its prompt on the standard error,
  // we need to redirect its standard error to the PDIP terminal
  // as we will use the shell prompt as outstanding data for the
  // following tests
  //
  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags = PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "/bin/sh";
  av[1] = NULL;
  rc = pdip_exec(pdip_1, 1, av);
  ck_assert_int_gt(rc, 1);

  // Wait for a regex which will not be found
  // ==> The prompt of the shell will go inside the outstanding data
  data_sz = 0;
  ck_assert_ptr_ne(display, 0);
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^qwerty =$", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_TIMEOUT);
  ck_assert_uint_eq(data_sz, 0);

  // Flush outstanding data
  display_sz = 20;
  data_sz = 0;
  display = (char *)malloc(display_sz);
  rc = pdip_flush(pdip_1, &display, &display_sz, &data_sz);
  ck_assert_int_eq(rc, 0);
  ck_assert_uint_gt(data_sz, 0);
  ck_assert_uint_gt(display_sz, 0);
  free(display);


  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

END_TEST




// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_delete)

int               rc;
pdip_t            pdip_1, pdip_2, pdip_3, pdip_4;
char             *display;
size_t            display_sz;
size_t            data_sz;
char             *av[2];
struct timeval    timeout;
pdip_cfg_t        cfg;
int               status;

  display_sz = 0;
  display = (char *)0;

  rc = pdip_configure(1, 0);
  ck_assert_int_eq(rc, 0);


  //
  // Delete object without controlled program
  //

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags = PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);


  //
  // Delete object with controlled
  //

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags = PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

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

  rc = pdip_delete(pdip_1, &status);
  ck_assert_int_eq(rc, 0);
  ck_assert(WIFSIGNALED(status));
  ck_assert_int_eq(WTERMSIG(status), SIGKILL);


  //
  // Delete object with outstanding data
  //

  //
  // As the shell sends its prompt on the standard error,
  // we need to redirect its standard error to the PDIP terminal
  // as we will use the shell prompt as outstanding data for the
  // following tests
  //
  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags = PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "/bin/sh";
  av[1] = NULL;
  rc = pdip_exec(pdip_1, 1, av);
  ck_assert_int_gt(rc, 1);

  // Wait for a regex which will not be found
  // ==> The prompt of the shell will go inside the outstanding data
  data_sz = 0;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^qwerty =$", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_TIMEOUT);
  ck_assert_uint_eq(data_sz, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);


  // Delete multiple objects

  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);
  pdip_2 = pdip_new(&cfg);
  ck_assert(pdip_2 != NULL);
  pdip_3 = pdip_new(&cfg);
  ck_assert(pdip_3 != NULL);
  pdip_4 = pdip_new(&cfg);
  ck_assert(pdip_4 != NULL);

  rc = pdip_delete(pdip_4, NULL);
  ck_assert_int_eq(rc, 0);
  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);
  rc = pdip_delete(pdip_3, NULL);
  ck_assert_int_eq(rc, 0);
  rc = pdip_delete(pdip_2, NULL);
  ck_assert_int_eq(rc, 0);

END_TEST


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_fd)

int               rc;
pdip_t            pdip_1;
pdip_cfg_t        cfg;
char             *av[2];
char             *display;
size_t            display_sz;
size_t            data_sz;
struct timeval    timeout;

  display_sz = 0;
  display = (char *)0;

  rc = pdip_configure(1, 0);
  ck_assert_int_eq(rc, 0);

  rc = pdip_configure(1, 0);
  ck_assert_int_eq(rc, 0);

  //
  // As the shell sends its prompt on the standard error,
  // we need to redirect its standard error to the PDIP terminal
  // as we will use the shell prompt as outstanding data for the
  // following tests
  //
  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags = PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  rc = setenv("PS1", CK_PDIP_PROMPT, 1);
  ck_assert_int_eq(rc, 0);

  pdip_set_debug_level(pdip_1, 20);

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

  rc = pdip_fd(pdip_1);
  ck_assert_int_ge(rc, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

END_TEST



// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_term_settings)

pdip_t          pdip_1;
char           *av[2];
int             rc;
char           *display;
size_t          display_sz;
size_t          data_sz;
struct timeval  timeout;
pdip_cfg_t      cfg;

extern void pdip_term_settings(pdip_t ctx);

  display_sz = 0;
  display = (char *)0;

  rc = pdip_configure(1, 0);
  ck_assert_int_eq(rc, 0);

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags = PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  rc = setenv("PS1", CK_PDIP_PROMPT, 1);
  ck_assert_int_eq(rc, 0);

  av[0] = "/bin/sh";
  av[1] = NULL;
  rc = pdip_exec(pdip_1, 1, av);
  ck_assert_int_gt(rc, 1);

  pdip_term_settings(pdip_1);

  // Wait for the prompt to make sure that the program is started
  data_sz = 0;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^" CK_PDIP_PROMPT "$", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_FOUND);
  ck_assert_uint_gt(data_sz, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

END_TEST




// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_status)

pdip_t          pdip_1;
char           *av[3];
int             rc;
char           *display;
size_t          display_sz;
size_t          data_sz;
struct timeval  timeout;
pdip_cfg_t      cfg;
int             status;

  display_sz = 0;
  display = (char *)0;

  rc = pdip_configure(1, 0);
  ck_assert_int_eq(rc, 0);


  // Wait for a living process in non blocking mode

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags = PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

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

  rc = pdip_status(pdip_1, &status, 0);
  ck_assert_int_eq(rc, -1);
  ck_assert_errno_eq(EAGAIN);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);


  //
  // Wait a terminated object
  //

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags = PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

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

  rc = pdip_sig(pdip_1, SIGKILL);
  ck_assert_int_eq(rc, 0);
  
  rc = pdip_status(pdip_1, &status, 1);
  ck_assert_int_eq(rc, 0);
  ck_assert(WIFSIGNALED(status));
  ck_assert_int_eq(WTERMSIG(status), SIGKILL);

  rc = pdip_status(pdip_1, &status, 0);
  ck_assert_int_eq(rc, 0);
  ck_assert(WIFSIGNALED(status));
  ck_assert_int_eq(WTERMSIG(status), SIGKILL);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);  


  // Wait for a process which terminates in blocking mode

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags = PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "/bin/sleep";
  av[1] = "1";
  av[2] = NULL;
  rc = pdip_exec(pdip_1, 2, av);
  ck_assert_int_gt(rc, 1);

  rc = pdip_status(pdip_1, &status, 1);
  ck_assert_int_eq(rc, 0);
  ck_assert_int_eq(status, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

END_TEST





// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_set_debug_level)

pdip_t          pdip_1;
char           *av[3];
int             rc;
char           *display;
size_t          display_sz;
size_t          data_sz;
struct timeval  timeout;
pdip_cfg_t      cfg;

  rc = pdip_configure(1, 0);
  ck_assert_int_eq(rc, 0);

  // Without object
  rc = pdip_set_debug_level(0, 1);
  ck_assert_int_eq(rc, 0);


  // With object

  display_sz = 0;
  display = (char *)0;

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags = PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

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

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

END_TEST



// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_cpu_nb)

unsigned int rcu;
int          rc;

  // Get the number of CPUs
  rc = sysconf(_SC_NPROCESSORS_ONLN);
  ck_assert_int_gt(rc, 0);

  rcu = pdip_cpu_nb();
  ck_assert_int_gt(rcu, 0);

  ck_assert_uint_eq(rc, rcu);

END_TEST



// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_cpu_alloc)

unsigned char *cpu;
int            rc;
pdip_t         pdip_1;
pdip_cfg_t      cfg;

  rc = pdip_configure(1, 0);
  ck_assert_int_eq(rc, 0);

  cpu = pdip_cpu_alloc();
  ck_assert_ptr_ne(cpu, 0);

  rc = pdip_cpu_free(cpu);
  ck_assert_int_eq(rc, 0);


  // Free an object with a CPU bitmap

  cpu = pdip_cpu_alloc();
  ck_assert_ptr_ne(cpu, 0);
  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.cpu = cpu;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

  rc = pdip_cpu_free(cpu);
  ck_assert_int_eq(rc, 0);  

END_TEST


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_cpu_zero)

unsigned char *cpu;
int            rc;
unsigned int   i;

  cpu = pdip_cpu_alloc();
  ck_assert_ptr_ne(cpu, 0);

  rc = pdip_cpu_zero(cpu);
  ck_assert_int_eq(rc, 0);

  for (i = 0; i < pdip_cpu_nb(); i ++)
  {
    rc = pdip_cpu_isset(cpu, i);
    ck_assert_int_eq(rc, 0);
  } // End for

  rc = pdip_cpu_free(cpu);
  ck_assert_int_eq(rc, 0);

END_TEST


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_cpu_set)

unsigned char *cpu;
int            rc;
unsigned int   i;

  cpu = pdip_cpu_alloc();
  ck_assert_ptr_ne(cpu, 0);

  rc = pdip_cpu_zero(cpu);
  ck_assert_int_eq(rc, 0);

  for (i = 0; i < pdip_cpu_nb(); i ++)
  {
    rc = pdip_cpu_isset(cpu, i);
    ck_assert_int_eq(rc, 0);

    rc = pdip_cpu_set(cpu, i);
    ck_assert_int_eq(rc, 0);

    rc = pdip_cpu_isset(cpu, i);
    ck_assert_int_eq(rc, 1);
  } // End for

  rc = pdip_cpu_free(cpu);
  ck_assert_int_eq(rc, 0);

END_TEST


// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_cpu_unset)

unsigned char *cpu;
int            rc;
unsigned int   i;

  cpu = pdip_cpu_alloc();
  ck_assert_ptr_ne(cpu, 0);

  rc = pdip_cpu_all(cpu);
  ck_assert_int_eq(rc, 0);

  for (i = 0; i < pdip_cpu_nb(); i ++)
  {
    rc = pdip_cpu_isset(cpu, i);
    ck_assert_int_eq(rc, 1);

    rc = pdip_cpu_unset(cpu, i);
    ck_assert_int_eq(rc, 0);

    rc = pdip_cpu_isset(cpu, i);
    ck_assert_int_eq(rc, 0);
  } // End for

  rc = pdip_cpu_free(cpu);
  ck_assert_int_eq(rc, 0);

END_TEST



// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_signal_hdl)

int               rc;
pdip_t            pdip_1, pdip_2, pdip_3, pdip_4, pdip_5, pdip_6;
char             *av[3];
int               status;

  rc = pdip_configure(1, 0);
  ck_assert_int_eq(rc, 0);

  // Create multiple object with processes which dies
  // to trigger the signal handler with a list of multiple living objects

  pdip_1 = pdip_new(0);
  ck_assert(pdip_1 != NULL);
  pdip_2 = pdip_new(0);
  ck_assert(pdip_2 != NULL);
  pdip_3 = pdip_new(0);
  ck_assert(pdip_3 != NULL);
  pdip_4 = pdip_new(0);
  ck_assert(pdip_4 != NULL);
  pdip_5 = pdip_new(0);
  ck_assert(pdip_5 != NULL);
  pdip_6 = pdip_new(0);
  ck_assert(pdip_6 != NULL);

  av[0] = "/bin/sleep";
  av[1] = "1";
  av[2] = NULL;
  rc = pdip_exec(pdip_1, 2, av);
  ck_assert_int_gt(rc, 1);
  
  av[0] = "/bin/sleep";
  av[1] = "1";
  av[2] = NULL;
  rc = pdip_exec(pdip_2, 2, av);
  ck_assert_int_gt(rc, 1);

  av[0] = "/bin/sleep";
  av[1] = "2";
  av[2] = NULL;
  rc = pdip_exec(pdip_3, 2, av);
  ck_assert_int_gt(rc, 1);

  av[0] = "/bin/sleep";
  av[1] = "1";
  av[2] = NULL;
  rc = pdip_exec(pdip_4, 2, av);
  ck_assert_int_gt(rc, 1);

  av[0] = "/bin/sleep";
  av[1] = "1";
  av[2] = NULL;
  rc = pdip_exec(pdip_5, 2, av);
  ck_assert_int_gt(rc, 1);

  av[0] = "/bin/sleep";
  av[1] = "1";
  av[2] = NULL;
  rc = pdip_exec(pdip_6, 2, av);
  ck_assert_int_gt(rc, 1);

  rc = pdip_status(pdip_1, 0, 1);
  ck_assert_int_eq(rc, 0);
  rc = pdip_status(pdip_2, 0, 1);
  ck_assert_int_eq(rc, 0);
  rc = pdip_status(pdip_3, 0, 1);
  ck_assert_int_eq(rc, 0);
  rc = pdip_status(pdip_4, 0, 1);
  ck_assert_int_eq(rc, 0);
  rc = pdip_status(pdip_5, 0, 1);
  ck_assert_int_eq(rc, 0);
  rc = pdip_status(pdip_6, 0, 1);
  ck_assert_int_eq(rc, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);
  rc = pdip_delete(pdip_2, NULL);
  ck_assert_int_eq(rc, 0);
  rc = pdip_delete(pdip_3, NULL);
  ck_assert_int_eq(rc, 0);
  rc = pdip_delete(pdip_4, NULL);
  ck_assert_int_eq(rc, 0);
  rc = pdip_delete(pdip_5, NULL);
  ck_assert_int_eq(rc, 0);
  rc = pdip_delete(pdip_6, NULL);
  ck_assert_int_eq(rc, 0);


  // Create a process which dies prematurely

  pdip_1 = pdip_new(0);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "/qwerty/not_exist/azerty";
  av[1] = "2";
  av[2] = NULL;
  rc = pdip_exec(pdip_1, 2, av);
  ck_assert_int_gt(rc, 1);

  // Wait for the end of the process
  rc = pdip_status(pdip_1, &status, 1);
  ck_assert_int_eq(rc, 0);
  ck_assert(WIFEXITED(status));
  // PDIP child exit(1) if exec() failed
  ck_assert_int_eq(WEXITSTATUS(status), 1);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

END_TEST



// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_cpu_all)

unsigned char *cpu;
int            rc;
unsigned int   i;

  cpu = pdip_cpu_alloc();
  ck_assert_ptr_ne(cpu, 0);

  rc = pdip_cpu_all(cpu);
  ck_assert_int_eq(rc, 0);

  for (i = 0; i < pdip_cpu_nb(); i ++)
  {
    rc = pdip_cpu_isset(cpu, i);
    ck_assert_int_eq(rc, 1);
  } // End for

  rc = pdip_cpu_free(cpu);
  ck_assert_int_eq(rc, 0);

END_TEST




// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_new)

unsigned char *cpu;
int            rc;
pdip_cfg_t     cfg;
pdip_t         pdip_1;
FILE          *out;

  rc = pdip_configure(1, 0);
  ck_assert_int_eq(rc, 0);

  // Create an object with default config

  pdip_1 = pdip_new((pdip_cfg_t *)0);
  ck_assert(pdip_1 != NULL);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);   

  // Create an object with a config set with default values

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);   

  // Create an object with all the fields configured by the user

  cpu = pdip_cpu_alloc();
  ck_assert_ptr_ne(cpu, 0);
  rc = pdip_cpu_set(cpu, 0);
  ck_assert_int_eq(rc, 0);

  out = fopen("/dev/null", "r+");
  ck_assert_ptr_ne(out, 0);

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.dbg_output            = out;
  cfg.err_output            = out;
  cfg.debug_level           = 10;
  cfg.flags                |= (PDIP_FLAG_ERR_REDIRECT | PDIP_FLAG_RECV_ON_THE_FLOW);
  cfg.cpu                   = cpu;
  cfg.buf_resize_increment  = 512;

  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);   

  rc = fclose(out);
  ck_assert_int_eq(rc, 0);

END_TEST



// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_exec)

int             rc;
pdip_cfg_t      cfg;
pdip_t          pdip_1;
char           *av[3];
unsigned char  *cpu;
int             status;
char           *display;
size_t          display_sz;
size_t          data_sz;
struct timeval  timeout;

  display_sz = 0;
  display = (char *)0;

  rc = pdip_configure(1, 0);
  ck_assert_int_eq(rc, 0);
  
  // Execute a program on an object for which the current controlled
  // process is dead but not freed

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  av[0] = "/bin/sleep";
  av[1] = "1";
  av[2] = NULL;
  rc = pdip_exec(pdip_1, 2, av);
  ck_assert_int_gt(rc, 1);

  // Wait for the end of the process
  rc = pdip_status(pdip_1, 0, 1);
  ck_assert_int_eq(rc, 0);

  av[0] = "/bin/sleep";
  av[1] = "1";
  av[2] = NULL;
  rc = pdip_exec(pdip_1, 2, av);
  ck_assert_int_gt(rc, 1);

  // Wait for the end of the process
  rc = pdip_status(pdip_1, 0, 1);
  ck_assert_int_eq(rc, 0);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

  //
  // Execute a process with a process affinity without any processors
  //

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cpu = pdip_cpu_alloc();
  cfg.cpu = cpu;

  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  av[0] = "/bin/sleep";
  av[1] = "1";
  av[2] = NULL;
  rc = pdip_exec(pdip_1, 2, av);
  ck_assert_int_gt(rc, 1);
  
  rc = pdip_status(pdip_1, &status, 1);
  ck_assert_int_eq(rc, 0);
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);
  
  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

  rc = pdip_cpu_free(cpu);
  ck_assert_int_eq(rc, 0);


  //
  // Execute a process with a process affinity with at least one processor
  //

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cpu = pdip_cpu_alloc();
  rc = pdip_cpu_set(cpu, 0);
  ck_assert_int_eq(rc, 0);
  cfg.cpu = cpu;

  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "test/myaffinity";
  av[1] = NULL;
  rc = pdip_exec(pdip_1, 1, av);
  ck_assert_int_gt(rc, 1);

  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^CPU: 0", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_FOUND);
  ck_assert_uint_gt(data_sz, 0);
  ck_assert_uint_gt(display_sz, 0);

  free(display);
  display_sz = 0;
  display = (char *)0;

  rc = pdip_status(pdip_1, &status, 1);
  ck_assert_int_eq(rc, 0);
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);
  
  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

  rc = pdip_cpu_free(cpu);
  ck_assert_int_eq(rc, 0);


  //
  // Execute a process with a process affinity with all the processors
  //

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cpu = pdip_cpu_alloc();
  rc = pdip_cpu_all(cpu);
  ck_assert_int_eq(rc, 0);
  cfg.cpu = cpu;

  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

  av[0] = "test/myaffinity";
  av[1] = NULL;
  rc = pdip_exec(pdip_1, 1, av);
  ck_assert_int_gt(rc, 1);

  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  rc = pdip_recv(pdip_1, "^CPU: 0", &display, &display_sz, &data_sz, &timeout);
  ck_assert_int_eq(rc, PDIP_RECV_FOUND);
  ck_assert_uint_gt(data_sz, 0);
  ck_assert_uint_gt(display_sz, 0);

  free(display);
  display_sz = 0;
  display = (char *)0;

  rc = pdip_status(pdip_1, &status, 1);
  ck_assert_int_eq(rc, 0);
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);
  
  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);

  rc = pdip_cpu_free(cpu);
  ck_assert_int_eq(rc, 0);

END_TEST




// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_sig)

int             rc;
pdip_cfg_t      cfg;
pdip_t          pdip_1;
char           *av[3];
int             status;
char           *display;
size_t          display_sz;
size_t          data_sz;
struct timeval  timeout;

  display_sz = 0;
  display = (char *)0;

  rc = pdip_configure(1, 0);
  ck_assert_int_eq(rc, 0);

  //
  // Kill a running process
  //

  rc = pdip_cfg_init(&cfg);
  ck_assert_int_eq(rc, 0);
  cfg.flags = PDIP_FLAG_ERR_REDIRECT;
  pdip_1 = pdip_new(&cfg);
  ck_assert(pdip_1 != NULL);

  pdip_set_debug_level(pdip_1, 20);

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

  rc = pdip_sig(pdip_1, SIGKILL);
  ck_assert_int_eq(rc, 0);
  
  rc = pdip_status(pdip_1, &status, 1);
  ck_assert_int_eq(rc, 0);
  ck_assert(WIFSIGNALED(status));
  ck_assert_int_eq(WTERMSIG(status), SIGKILL);

  rc = pdip_delete(pdip_1, NULL);
  ck_assert_int_eq(rc, 0);    

END_TEST



// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_pdip_dump)

char         *buf;
unsigned int  i;

extern void pdip_dump(char   *buf, size_t  size_buf);

  pdip_dump(0, 0);

  buf = malloc(1);
  ck_assert_ptr_ne(buf, 0);
  buf[0] = 12;
  pdip_dump(buf, 1);
  free(buf);

  buf = malloc(15);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 15; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 15);
  free(buf);


  buf = malloc(16);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 16; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 16);
  free(buf);

  buf = malloc(17);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 17; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 17);
  free(buf);

  buf = malloc(18);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 18; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 18);
  free(buf);

  buf = malloc(19);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 19; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 19);
  free(buf);

  buf = malloc(500);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 500; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 500);
  free(buf);

  buf = malloc(511);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 511; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 511);
  free(buf);

  buf = malloc(512);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 512; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 512);
  free(buf);

  buf = malloc(513);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 513; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 513);
  free(buf);

  buf = malloc(514);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 514; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 514);
  free(buf);

  buf = malloc(1023);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 1023; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 1023);
  free(buf);


  buf = malloc(1024);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 1024; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 1024);
  free(buf);

  buf = malloc(1025);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 1025; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 1025);
  free(buf);

  buf = malloc(1026);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 1026; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 1026);
  free(buf);

  buf = malloc(1027);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 1027; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 1027);
  free(buf);


  buf = malloc(1028);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 1028; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 1028);
  free(buf);


  buf = memalign(2, 253);
  fprintf(stderr, "buf=%p (2)\n", buf);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 253; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 253);
  free(buf);


  buf = memalign(4, 253);
  fprintf(stderr, "buf=%p (4)\n", buf);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 253; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 253);
  free(buf);

  buf = memalign(8, 253);
  fprintf(stderr, "buf=%p (8)\n", buf);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 253; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 253);
  free(buf);

  buf = memalign(16, 253);
  fprintf(stderr, "buf=%p (16)\n", buf);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 253; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 253);
  free(buf);

  buf = memalign(32, 253);
  fprintf(stderr, "buf=%p (32)\n", buf);
  ck_assert_ptr_ne(buf, 0);
  for (i = 0; i < 253; i ++)
  {
    buf[i] = i;
  } // End for
  pdip_dump(buf, 253);
  free(buf);

END_TEST



// The unitary tests are run in separate processes
// START_TEST is a "static" definition of a function
START_TEST(test_man)

int status;

  status = system("test/man_exe_1");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

  status = system("test/man_exe_2 4+7");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

  status = system("test/man_exe_3");
  ck_assert(WIFEXITED(status));
  ck_assert_int_eq(WEXITSTATUS(status), 0);

END_TEST




TCase *pdip_api_tests(void)
{
TCase *tc_api;

  printf("%s: %d\n", __FUNCTION__, getpid());

  /* Error test case */
  tc_api = tcase_create("PDIP API");

  // Increase the timeout for the test case as we use lots of timeouts
  tcase_set_timeout(tc_api, 60);

  tcase_add_unchecked_fixture(tc_api, pdip_setup_unchecked_fixture, 0);
  tcase_add_checked_fixture(tc_api, pdip_setup_checked_fixture, 0);
  tcase_add_test(tc_api, test_pdip_recv);
  //tcase_add_test_raise_signal(tc_api, test_pdip_recv, SIGALRM);
  tcase_add_test(tc_api, test_pdip_send);
  tcase_add_test(tc_api, test_pdip_flush);
  tcase_add_test(tc_api, test_pdip_delete);
  tcase_add_test(tc_api, test_pdip_fd);
  tcase_add_test(tc_api, test_pdip_term_settings);
  tcase_add_test(tc_api, test_pdip_status);
  tcase_add_test(tc_api, test_pdip_set_debug_level);
  tcase_add_test(tc_api, test_pdip_cpu_nb);
  tcase_add_test(tc_api, test_pdip_cpu_alloc);
  tcase_add_test(tc_api, test_pdip_cpu_zero);
  tcase_add_test(tc_api, test_pdip_cpu_all);
  tcase_add_test(tc_api, test_pdip_cpu_set);
  tcase_add_test(tc_api, test_pdip_cpu_unset);
  tcase_add_test(tc_api, test_signal_hdl);
  tcase_add_test(tc_api, test_pdip_new);
  tcase_add_test(tc_api, test_pdip_exec);
  tcase_add_test(tc_api, test_pdip_sig);
  tcase_add_test(tc_api, test_pdip_dump);
  tcase_add_test(tc_api, test_man);

  return tc_api;
} // pdip_api_tests
