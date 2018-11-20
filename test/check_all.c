// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : check_all.c
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



#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


#include "check_all.h"
#include "check_pdip.h"
#include "check_isys.h"
#include "check_rsys.h"




int main(void)
{
int      number_failed = 0;
Suite   *s1, *s2, *s3;
SRunner *sr;

  printf("%s: %d\n", __FUNCTION__, getpid());

  // Create the suites
  s1 = suite_create("PDIP tests");
  s2 = suite_create("ISYS tests");
  s3 = suite_create("RSYS tests");

  // Add the tests cases in the suites
  suite_add_tcase(s1, pdip_errcodes_tests());
  suite_add_tcase(s1, pdip_api_tests());
  suite_add_tcase(s2, isys_errcodes_tests());
  suite_add_tcase(s2, isys_api_tests());
  suite_add_tcase(s2, isys_api_env_tests());
  suite_add_tcase(s3, rsys_errcodes_tests());
  suite_add_tcase(s3, rsys_api_tests());
  suite_add_tcase(s3, rsys_rsystemd_tests());

  // Startup of RSYS servers
  rsys_startup();

  sr = srunner_create(s1);
  srunner_add_suite(sr, s2);
  srunner_add_suite(sr, s3);

  srunner_run_all(sr, CK_ENV);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);

  // End of RSYS servers
  rsys_end();

  return (0 == number_failed) ? EXIT_SUCCESS : EXIT_FAILURE;
} // main

