// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : check_pdip.c
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



#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


#include "check_pdip.h"




int main(void)
{
int      number_failed = 0;
Suite   *s;
SRunner *sr;

  printf("%s: %d\n", __FUNCTION__, getpid());

  s = suite_create("PDIP tests");
  suite_add_tcase(s, pdip_errcodes_tests());
  suite_add_tcase(s, pdip_api_tests());

  sr = srunner_create(s);

  srunner_run_all(sr, CK_ENV);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (0 == number_failed) ? EXIT_SUCCESS : EXIT_FAILURE;
} // main

